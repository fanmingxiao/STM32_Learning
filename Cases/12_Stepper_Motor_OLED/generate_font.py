# coding: utf-8
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image, ImageDraw, ImageFont

# 我们需要处理的字符列表
chars = ["当", "前", "角", "度", "正", "反", "转"]

# 尝试寻找系统自带的支持中文的字体
font_paths = [
    '/System/Library/Fonts/PingFang.ttc',
    '/System/Library/Fonts/STHeiti Light.ttc',
    '/System/Library/Fonts/Hiragino Sans GB.ttc',
    '/Library/Fonts/Arial Unicode.ttf'
]

font = None
for fp in font_paths:
    if os.path.exists(fp):
        font = ImageFont.truetype(fp, 15)  # Use 15 or 16 to fit nicely in 16x16
        break

if font is None:
    font = ImageFont.load_default()

out_code = """
#ifndef __OLED_CHINESE_H
#define __OLED_CHINESE_H

typedef struct {
    unsigned char index[4];
    unsigned char dat[32];
} ChineseFont;

const ChineseFont CHN_FONTS[] = {
"""

for char in chars:
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    # PIL draw.text anchors at top-left by default for basic fonts, or use anchor='la'
    # Actually, we can just center it.
    try:
        bbox = font.getbbox(char)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
    except AttributeError:
        w, h = font.getsize(char)
        
    x = (16 - w) // 2
    y = (16 - h) // 2 - 2  # slight vertical offset tweak
    if y < 0: y = 0
    
    draw.text((x, y), char, font=font, fill=1)
    
    pixels = list(img.getdata())
    matrix = []
    
    # y=0 to 7 (Top page), columns x=0 to 15
    for px in range(16):
        byte = 0
        for py in range(8):
            if pixels[py * 16 + px]:
                byte |= (1 << py)  # LSB is top (y=0 is bit 0)
        matrix.append(byte)
    
    # y=8 to 15 (Bottom page), columns x=0 to 15
    for px in range(16):
        byte = 0
        for py in range(8, 16):
            if pixels[py * 16 + px]:
                byte |= (1 << (py - 8))  # LSB is y=8
        matrix.append(byte)
        
    v_str = ",".join([f"0x{b:02X}" for b in matrix])
    k_bytes = char.encode('utf-8')
    
    if len(k_bytes) == 3:
        out_code += f"    {{\"\\x{k_bytes[0]:02X}\\x{k_bytes[1]:02X}\\x{k_bytes[2]:02X}\", {{{v_str}}}}}, // {char}\n"
    elif len(k_bytes) == 1:
        out_code += f"    {{\"{char}\", {{{v_str}}}}}, // {char}\n"

out_code += """};
#define CHN_FONTS_LEN (sizeof(CHN_FONTS)/sizeof(ChineseFont))
#endif
"""

with open("include/oled_chinese.h", "w", encoding='utf-8') as f:
    f.write(out_code)

print("Generated.")

