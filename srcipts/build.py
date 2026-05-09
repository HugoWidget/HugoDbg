import sys
import pathlib
import jsmin
import brotli
import secrets
import json

def get_base_dir() -> pathlib.Path:
    if getattr(sys, 'frozen', False):
        return pathlib.Path(sys.executable).parent
    return pathlib.Path(__file__).parent

BASE_DIR = get_base_dir()
SRC_DIR = BASE_DIR / "frontend"
BUILD_DIR = BASE_DIR / "build"

BUNDLE_FILES = [
    'selfcheck.js', 'divWindow.js', 'divDialog.js',
    'miniConsole.js', 'vKeyboard.js', 'main.js'
]

Z85_ENCODER_TABLE = list("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#")
Z85_DECODER_TABLE_USER = [
    0x00, 0x44, 0x00, 0x54, 0x53, 0x52, 0x48, 0x00,
    0x4B, 0x4C, 0x46, 0x41, 0x00, 0x3F, 0x3E, 0x45,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x40, 0x00, 0x49, 0x42, 0x4A, 0x47,
    0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
    0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0x4D, 0x00, 0x4E, 0x43, 0x00,
    0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x4F, 0x00, 0x50, 0x00, 0x00
]

CHUNK_SIZE = 768


def z85_encode_strict(data: bytes) -> str:
    if len(data) % 4 != 0:
        raise ValueError("z85_encode_strict input length must be a multiple of 4.")
    out = ""
    value = 0
    for i, b in enumerate(data):
        value = value * 256 + b
        if (i + 1) % 4 == 0:
            divisor = 52200625  # 85^4
            for _ in range(5):
                out += Z85_ENCODER_TABLE[value // divisor % 85]
                divisor //= 85
            value = 0
    return out

def to_c_string_literal(js_str: str) -> str:
    """转义 '??' 为 C 字符串字面量"""
    return json.dumps(js_str).replace("??", "?\\?")

def process_plugin_loader():
    input_path = SRC_DIR / "pluginLoader.js"
    output_path = BUILD_DIR / "pluginLoader.min.js"
    code = input_path.read_text(encoding='utf-8')
    minified = jsmin.jsmin(code)
    final = f'const pluginLoaderJS = "{minified}";'
    output_path.write_text(final, encoding='utf-8')
    return final

def process_bundle_and_generate_injection_commands(plugin_loader: str):
    # 拼接所有 JS
    codes = []
    for f in BUNDLE_FILES:
        codes.append((SRC_DIR / f).read_text(encoding='utf-8'))
    concatenated = '\n'.join(codes)
    concatenated = concatenated.replace('const pluginLoaderJS = "";', plugin_loader)
    iife = f'!function(){{\n{concatenated}\n}}();'
    original_script = jsmin.jsmin(iife)

    # Brotli 压缩
    compressed = brotli.compress(original_script.encode('utf-8'))
    orig_len = len(compressed)
    padding = (4 - (orig_len % 4)) % 4
    padded = compressed + b'\x00' * padding
    encoded = z85_encode_strict(padded)

    # 随机变量名，分块
    rand_suffix = secrets.token_hex(4)
    var_name = f'__c_{rand_suffix}'
    chunks = [encoded[i:i+CHUNK_SIZE] for i in range(0, len(encoded), CHUNK_SIZE)]

    # 构造注入命令
    def esc(s: str) -> str:
        return s.replace('\\', '\\\\').replace("'", "\\'")

    cmds = []
    if chunks:
        cmds.append(f"global.{var_name}=['{esc(chunks[0])}']")
        for ch in chunks[1:]:
            cmds.append(f"global.{var_name}.push('{esc(ch)}')")
    else:
        cmds.append(f"global.{var_name}=[]")

    decoder_table = json.dumps(Z85_DECODER_TABLE_USER)
    final_cmd = (
        f'(()=>{{const T={decoder_table};const D=s=>{{let b=Buffer.alloc(s.length*4/5),i=0,j=0,v=0;'
        f'while(j<s.length){{v=v*85+T[s.charCodeAt(j++)-32];if(j%5==0){{let d=16777216;while(d>=1)'
        f'{{b[i++]=v/d&255;d/=256}}v=0}}}}return b}};let r=D(global.{var_name}.join(\'\')).slice(0,{orig_len});'
        f'global.___j_(require(\'zlib\').brotliDecompressSync(r).toString());global.{var_name}=undefined;}})()'
    )
    cmds.append(final_cmd)

    (BUILD_DIR / "inject.json").write_text(json.dumps(cmds, indent=2), encoding='utf-8')
    (BUILD_DIR / "bundle.min.js").write_text(original_script, encoding='utf-8')
    return cmds

def process_test_file():
    input_path = SRC_DIR / "test.js"
    output_path = BUILD_DIR / "test.min.js"
    code = input_path.read_text(encoding='utf-8')
    minified = jsmin.jsmin(code)
    final = minified.replace('"', '`')
    output_path.write_text(final, encoding='utf-8')
    return final

def process_clean_file():
    input_path = SRC_DIR / "clean.js"
    output_path = BUILD_DIR / "clean.min.js"
    code = input_path.read_text(encoding='utf-8')
    minified = jsmin.jsmin(code)
    final = f'global.___i_(`{minified}`);global.___i_=global.___j_=global.___self__hugodbg_plugin_loader_=undefined;'
    output_path.write_text(final, encoding='utf-8')
    return final

def process_loader_file():
    input_path = SRC_DIR / "loader.js"
    output_path = BUILD_DIR / "loader.min.js"
    code = input_path.read_text(encoding='utf-8')
    minified = jsmin.jsmin(code)
    output_path.write_text(minified, encoding='utf-8')
    return minified

def generate_c_header(test_js: str, clean_js: str, injection_commands: list, loader_js: str):
    output_path = BUILD_DIR / "scripts.h"
    test_lit = to_c_string_literal(test_js)
    clean_lit = to_c_string_literal(clean_js)
    loader_lit = to_c_string_literal(loader_js)
    bundle_lit = ',\n    '.join(to_c_string_literal(cmd) for cmd in injection_commands)

    header = f"""/*
 * Auto-generated by script. Do not edit.
 */
#ifndef SCRIPTS_H
#define SCRIPTS_H
const char *test_js = {test_lit};
const char *clean_js = {clean_lit};
const char *bundle_js[] = {{
    {bundle_lit}
}};
const char *loader_js = {loader_lit};
#endif
"""
    output_path.write_text(header, encoding='utf-8')

def build():
    try:
        BUILD_DIR.mkdir(exist_ok=True)
        print(f"Base directory: {BASE_DIR.resolve()}")
        print(f"Source directory: {SRC_DIR.resolve()}")
        print(f"Build directory: {BUILD_DIR.resolve()}")

        plugin_loader = process_plugin_loader()
        injection_cmds = process_bundle_and_generate_injection_commands(plugin_loader)
        test_js = process_test_file()
        clean_js = process_clean_file()
        loader_js = process_loader_file()
        generate_c_header(test_js, clean_js, injection_cmds, loader_js)

        print('BUILD SUCCESS')
    except Exception as e:
        print(f'BUILD FAILED: {e}')
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    build()
