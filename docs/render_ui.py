from PIL import Image, ImageDraw, ImageFont
import os

# Colors (RGB565 to RGB)
BG = (0, 0, 0)
CARD = (24, 227, 30) # Wait, 0x18E3 is RGB565.
# Let's decode 0x18E3:
# R = (0x18E3 >> 11) & 0x1F -> 3 -> 3*8 = 24
# G = (0x18E3 >> 5) & 0x3F -> 7 -> 7*4 = 28
# B = 0x18E3 & 0x1F -> 3 -> 3*8 = 24
# Dark greyish. Let's use (24, 28, 24). Actually CARD is (24, 28, 24).
CARD = (25, 30, 25)
DIM = (100, 100, 100)
WHITE = (255, 255, 255)
C_GREEN = (0, 255, 0)
C_YELLOW = (255, 255, 0)
C_ORANGE = (255, 165, 0)
C_RED = (255, 50, 50)
C_BLUE = (100, 150, 255)
DARK = (40, 40, 40)

def draw_therm(draw, x, y, c):
    draw.ellipse([x+4-3, y+10-3, x+4+3, y+10+3], outline=c)
    draw.ellipse([x+4-2, y+10-2, x+4+2, y+10+2], fill=c)
    draw.line([x+3, y, x+3, y+10], fill=c)
    draw.line([x+5, y, x+5, y+10], fill=c)
    draw.point((x+4, y), fill=c)
    draw.line([x+3, y+6, x+5, y+6], fill=c)

def draw_drop(draw, x, y, c):
    draw.ellipse([x+4-4, y+8-4, x+4+4, y+8+4], fill=c)
    draw.polygon([(x+4, y), (x, y+8), (x+8, y+8)], fill=c)

def draw_flame(draw, x, y, c):
    draw.ellipse([x+4-4, y+8-4, x+4+4, y+8+4], fill=c)
    draw.polygon([(x+4, y), (x+1, y+8), (x+7, y+8)], fill=c)
    draw.ellipse([x+4-2, y+8-2, x+4+2, y+8+2], fill=BG)

def draw_door(draw, x, y, c):
    draw.rectangle([x, y, x+8, y+12], outline=c)
    draw.line([x+2, y, x+2, y+12], fill=c)
    draw.point((x+5, y+6), fill=c)

def draw_window(draw, x, y, c):
    draw.rectangle([x, y, x+10, y+12], outline=c)
    draw.line([x+4, y, x+4, y+12], fill=c)
    draw.line([x, y+5, x+10, y+5], fill=c)

def draw_stairs(draw, x, y, c):
    draw.line([x, y+10, x+3, y+10], fill=c)
    draw.line([x+3, y+7, x+3, y+11], fill=c)
    draw.line([x+3, y+7, x+6, y+7], fill=c)
    draw.line([x+6, y+4, x+6, y+8], fill=c)
    draw.line([x+6, y+4, x+10, y+4], fill=c)

def create_dashboard():
    img = Image.new('RGB', (240, 135), BG)
    draw = ImageDraw.Draw(img)
    
    # Left top: CO2
    draw.rounded_rectangle([2, 2, 132, 54], radius=4, fill=CARD)
    draw.text((10, 10), "850", fill=C_GREEN, font=None) 
    draw.text((40, 15), "ppm", fill=DIM)
    draw.text((100, 5), "Good", fill=C_GREEN)
    
    # CO2 Bar
    bx, by, bw, bh = 8, 44, 120, 6
    draw.rounded_rectangle([bx, by, bx+bw, by+bh], radius=2, fill=DARK)
    fill_ratio = 850.0 / 2000.0
    draw.rounded_rectangle([bx, by, bx+int(bw*fill_ratio), by+bh], radius=2, fill=C_GREEN)
    
    # Left bottom: Temp/Hum
    draw.rounded_rectangle([2, 58, 64, 110], radius=4, fill=CARD)
    draw_therm(draw, 10, 62, C_GREEN)
    draw.text((22, 63), "22.5 C", fill=C_GREEN)
    
    draw.rounded_rectangle([68, 58, 132, 110], radius=4, fill=CARD)
    draw_drop(draw, 76, 62, C_GREEN)
    draw.text((88, 63), "45 %", fill=C_GREEN)
    
    # Right: Alerts
    draw.rounded_rectangle([136, 2, 238, 110], radius=4, fill=CARD)
    ry = 5
    draw_flame(draw, 140, ry, DARK)
    draw.text((154, ry), "Heat", fill=DIM)
    ry += 20
    draw_door(draw, 140, ry, C_ORANGE)
    draw.text((154, ry), "Door 2m", fill=C_ORANGE)
    ry += 20
    draw_window(draw, 140, ry, C_BLUE)
    draw.text((154, ry), "Win1 14m", fill=C_BLUE)
    ry += 20
    draw_window(draw, 140, ry, DARK)
    draw.text((154, ry), "Win2", fill=DIM)
    ry += 20
    draw_stairs(draw, 140, ry, DARK)
    draw.text((154, ry), "Stairs", fill=DIM)
    
    # Bottom bar
    draw.text((10, 118), "WiFi MQTT", fill=C_GREEN)
    draw.text((200, 118), "14:30", fill=WHITE)
    
    # Resize up to make it readable in github markdown
    img = img.resize((480, 270), Image.NEAREST)
    img.save("docs/ui_dashboard.png")

def create_ha_detail():
    img = Image.new('RGB', (240, 135), BG)
    draw = ImageDraw.Draw(img)
    
    # Left: Status
    draw.rounded_rectangle([2, 2, 118, 110], radius=4, fill=CARD)
    ry = 5
    draw_flame(draw, 10, ry, DARK)
    draw.text((24, ry), "Heating OFF", fill=DIM)
    ry += 20
    draw_door(draw, 10, ry, C_ORANGE)
    draw.text((24, ry), "Door OPEN 2m", fill=C_ORANGE)
    ry += 20
    draw_window(draw, 10, ry, C_BLUE)
    draw.text((24, ry), "Win1 OPEN 14m", fill=C_BLUE)
    ry += 20
    draw_window(draw, 10, ry, DARK)
    draw.text((24, ry), "Win2 Closed", fill=DIM)
    ry += 20
    draw_stairs(draw, 10, ry, DARK)
    draw.text((24, ry), "Stairs Clear", fill=DIM)
    
    # Right: CO2/Temp/Hum
    draw.rounded_rectangle([122, 2, 238, 110], radius=4, fill=CARD)
    draw.text((130, 20), "CO2  850ppm", fill=C_GREEN)
    draw.text((130, 50), "Temp 22.5C", fill=C_GREEN)
    draw.text((130, 80), "Hum  45%", fill=C_GREEN)
    
    # Bottom bar
    draw.text((10, 118), "WiFi MQTT", fill=C_GREEN)
    draw.text((200, 118), "14:30", fill=WHITE)
    
    img = img.resize((480, 270), Image.NEAREST)
    img.save("docs/ui_ha_detail.png")

create_dashboard()
create_ha_detail()
print("Images rendered.")
