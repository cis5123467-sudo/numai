"""Draw the 55 by 56 launcher icon based on the reference's gold N mark."""
from pathlib import Path
from PIL import Image, ImageDraw

scale = 4
width, height = 55, 56
mask = Image.new("L", (width * scale, height * scale))
ImageDraw.Draw(mask).rounded_rectangle((4, 4, width*scale-5, height*scale-5), 11*scale, fill=255)
gold = Image.new("RGB", mask.size)
draw = ImageDraw.Draw(gold)
for y in range(height * scale):
    t = y / (height * scale - 1)
    draw.line((0, y, width * scale, y), fill=(249, round(202 - 32*t), round(87 - 28*t)))

mark = (255, 251, 227)
def point(p):
    return tuple(round(v * scale) for v in p)

draw.line([point(p) for p in ((16, 37), (16, 16), (38, 38), (38, 14))],
          fill=mark, width=4*scale, joint="curve")
for x, y in ((16, 38), (16, 16), (38, 38), (38, 14)):
    draw.ellipse((*point((x-3.5, y-3.5)), *point((x+3.5, y+3.5))), fill=mark)
icon = Image.new("RGB", mask.size, "white")
icon.paste(gold, mask=mask)
icon.resize((width, height), Image.Resampling.LANCZOS).save(Path(__file__).parents[1] / "src/icon.png")
