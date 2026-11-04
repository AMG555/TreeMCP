import os
import math
from PIL import Image, ImageDraw, ImageFilter

os.makedirs("assets", exist_ok=True)
os.makedirs("src/ui", exist_ok=True)
os.makedirs("src/resources", exist_ok=True)

# 1. GENERATE SVG LOGO
svg_content = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="100%" height="100%">
  <defs>
    <linearGradient id="bgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0a0e17"/>
      <stop offset="100%" stop-color="#141c2b"/>
    </linearGradient>
    <linearGradient id="borderGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#20bf6b"/>
      <stop offset="50%" stop-color="#0fb9b1"/>
      <stop offset="100%" stop-color="#3867d6"/>
    </linearGradient>
    <linearGradient id="trunkGrad" x1="0%" y1="100%" x2="0%" y2="0%">
      <stop offset="0%" stop-color="#0fb9b1"/>
      <stop offset="100%" stop-color="#20bf6b"/>
    </linearGradient>
    <linearGradient id="cyanTile" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#00d2d3"/>
      <stop offset="100%" stop-color="#01a3a4"/>
    </linearGradient>
    <linearGradient id="greenTile" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#26de81"/>
      <stop offset="100%" stop-color="#20bf6b"/>
    </linearGradient>
    <linearGradient id="blueTile" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#45aaf2"/>
      <stop offset="100%" stop-color="#2d98da"/>
    </linearGradient>
    <linearGradient id="amberTile" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#fed330"/>
      <stop offset="100%" stop-color="#f7b731"/>
    </linearGradient>
    <linearGradient id="purpleTile" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#a55eea"/>
      <stop offset="100%" stop-color="#8854d0"/>
    </linearGradient>
    <filter id="glow" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur stdDeviation="10" result="blur"/>
      <feComposite in="SourceGraphic" in2="blur" operator="over"/>
    </filter>
  </defs>

  <!-- App Rounded Base Container -->
  <rect x="16" y="16" width="480" height="480" rx="108" fill="url(#bgGrad)" stroke="url(#borderGrad)" stroke-width="8"/>

  <!-- Platter Track Concentric Arcs (Disk Roots) -->
  <path d="M 156 410 A 130 130 0 0 1 356 410" fill="none" stroke="#22324d" stroke-width="4" stroke-dasharray="8 6"/>
  <path d="M 126 426 A 170 170 0 0 1 386 426" fill="none" stroke="#1d2a40" stroke-width="4" stroke-dasharray="10 8"/>
  <circle cx="256" cy="420" r="14" fill="#0fb9b1" opacity="0.3"/>
  <circle cx="256" cy="420" r="7" fill="#20bf6b"/>

  <!-- Data Trunk & Branch Nodes (Neural MCP Backbone) -->
  <path d="M 256 420 L 256 220" stroke="url(#trunkGrad)" stroke-width="12" stroke-linecap="round"/>
  <path d="M 256 310 L 170 230" stroke="url(#trunkGrad)" stroke-width="8" stroke-linecap="round"/>
  <path d="M 256 310 L 342 230" stroke="url(#trunkGrad)" stroke-width="8" stroke-linecap="round"/>
  <path d="M 256 250 L 140 180" stroke="url(#trunkGrad)" stroke-width="7" stroke-linecap="round"/>
  <path d="M 256 250 L 372 180" stroke="url(#trunkGrad)" stroke-width="7" stroke-linecap="round"/>

  <!-- Treemap Space Partition Tiles (Tree Crown) -->
  <!-- Top Center Primary Green Tile -->
  <rect x="206" y="80" width="100" height="85" rx="12" fill="url(#greenTile)" filter="url(#glow)"/>
  <rect x="216" y="90" width="40" height="30" rx="6" fill="#ffffff" opacity="0.25"/>

  <!-- Top Left Cyan Tile -->
  <rect x="110" y="100" width="84" height="74" rx="12" fill="url(#cyanTile)"/>
  <rect x="118" y="108" width="30" height="25" rx="6" fill="#ffffff" opacity="0.25"/>

  <!-- Top Right Sapphire Tile -->
  <rect x="318" y="100" width="84" height="74" rx="12" fill="url(#blueTile)"/>
  <rect x="326" y="108" width="30" height="25" rx="6" fill="#ffffff" opacity="0.25"/>

  <!-- Mid Left Amber Tile -->
  <rect x="75" y="190" width="80" height="65" rx="10" fill="url(#amberTile)"/>
  <rect x="83" y="198" width="28" height="20" rx="5" fill="#ffffff" opacity="0.25"/>

  <!-- Mid Left Nested Emerald Tile -->
  <rect x="165" y="190" width="75" height="65" rx="10" fill="url(#greenTile)"/>

  <!-- Mid Right Purple Tile (MCP Neural) -->
  <rect x="357" y="190" width="80" height="65" rx="10" fill="url(#purpleTile)"/>
  <rect x="365" y="198" width="28" height="20" rx="5" fill="#ffffff" opacity="0.25"/>

  <!-- Mid Right Nested Cyan Tile -->
  <rect x="272" y="190" width="75" height="65" rx="10" fill="url(#cyanTile)"/>

  <!-- Lower Left Sub-tile -->
  <rect x="120" y="270" width="60" height="48" rx="8" fill="url(#blueTile)"/>

  <!-- Lower Right Sub-tile -->
  <rect x="332" y="270" width="60" height="48" rx="8" fill="url(#amberTile)"/>

  <!-- Connection Nodes / Spark Rings (MCP Active Indicators) -->
  <circle cx="206" cy="190" r="5" fill="#ffffff"/>
  <circle cx="306" cy="190" r="5" fill="#ffffff"/>
  <circle cx="256" cy="165" r="5" fill="#ffffff"/>
  <circle cx="150" cy="270" r="4" fill="#ffffff"/>
  <circle cx="362" cy="270" r="4" fill="#ffffff"/>
</svg>'''

with open("assets/logo.svg", "w", encoding="utf-8") as f:
    f.write(svg_content)
with open("src/ui/logo.svg", "w", encoding="utf-8") as f:
    f.write(svg_content)

print("[OK] Vector SVG logo generated at assets/logo.svg and src/ui/logo.svg")

# 2. GENERATE HIGH-RES RASTER IMAGE & WINDOWS MULTI-SIZE ICO
size = 512
img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)

# Background rounded squircle
bg_box = [20, 20, 492, 492]
draw.rounded_rectangle(bg_box, radius=110, fill=(14, 20, 32, 255), outline=(32, 191, 107, 255), width=8)

# Subtle concentric platter tracks
for r, dash in [(140, 4), (180, 5)]:
    bbox = [256 - r, 430 - r, 256 + r, 430 + r]
    draw.arc(bbox, start=190, end=350, fill=(40, 60, 90, 180), width=dash)

# Disk Hub
draw.ellipse([256 - 16, 420 - 16, 256 + 16, 420 + 16], fill=(15, 185, 177, 120))
draw.ellipse([256 - 8, 420 - 8, 256 + 8, 420 + 8], fill=(32, 191, 107, 255))

# Tree Trunk & Branches
draw.line([(256, 420), (256, 220)], fill=(32, 191, 107, 255), width=12)
draw.line([(256, 310), (170, 230)], fill=(15, 185, 177, 255), width=8)
draw.line([(256, 310), (342, 230)], fill=(15, 185, 177, 255), width=8)
draw.line([(256, 250), (140, 180)], fill=(32, 191, 107, 255), width=7)
draw.line([(256, 250), (372, 180)], fill=(32, 191, 107, 255), width=7)

# Treemap Tiles (Boxes)
tiles = [
    ([206, 75, 306, 165], 14, (38, 222, 129, 255)),  # Top Center Emerald
    ([110, 95, 195, 172], 12, (0, 210, 211, 255)),   # Top Left Cyan
    ([317, 95, 402, 172], 12, (69, 170, 242, 255)),  # Top Right Sapphire
    ([75, 185, 155, 252], 10, (254, 211, 48, 255)),  # Mid Left Amber
    ([165, 185, 242, 252], 10, (32, 191, 107, 255)), # Mid Left Emerald
    ([357, 185, 437, 252], 10, (165, 94, 234, 255)), # Mid Right Purple
    ([270, 185, 347, 252], 10, (15, 185, 177, 255)), # Mid Right Cyan
    ([120, 268, 182, 318], 8, (45, 152, 218, 255)),  # Lower Left Blue
    ([330, 268, 392, 318], 8, (247, 183, 49, 255)),  # Lower Right Amber
]

for box, rad, color in tiles:
    draw.rounded_rectangle(box, radius=rad, fill=color)
    # Glass highlight reflection
    hl = [box[0] + 6, box[1] + 6, box[0] + (box[2] - box[0]) * 0.45, box[1] + (box[3] - box[1]) * 0.35]
    draw.rounded_rectangle(hl, radius=max(2, rad - 4), fill=(255, 255, 255, 70))

# Connection Nodes
nodes = [(206, 185), (306, 185), (256, 165), (151, 268), (361, 268)]
for nx, ny in nodes:
    draw.ellipse([nx - 5, ny - 5, nx + 5, ny + 5], fill=(255, 255, 255, 255))

# Save 512x512 Master PNG
img.save("assets/logo.png", format="PNG")
print("[OK] Master PNG logo saved at assets/logo.png")

# Save Windows Multi-Resolution ICO
ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
img.save("assets/icon.ico", format="ICO", sizes=ico_sizes)
img.save("src/ui/favicon.ico", format="ICO", sizes=[(32, 32), (16, 16)])

print("[OK] Windows multi-size icon saved at assets/icon.ico")
print("[OK] Favicon saved at src/ui/favicon.ico")

