
import io
import random
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

import requests
from PIL import Image

# =========================================================
# CONFIG
# =========================================================

LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 8080

# API ключ хранится ТОЛЬКО здесь
FIREWORKS_API_KEY = "fw_3ZXJgqbCd3UWHSZTNfpQCxAZ"

# Fireworks endpoint
FW_URL = (
    "https://api.fireworks.ai/inference/v1/image_generation/"
    "accounts/fireworks/models/stable-diffusion-xl-1024-v1-0"
)

# Proxy (выбери нужный)
# PROXY_URL = "socks5h://127.0.0.1:10808"
PROXY_URL = "http://127.0.0.1:10809"
PROXIES = {"http": PROXY_URL, "https": PROXY_URL}

# Open-Meteo (ТОЧНО под твой ответ)
OPEN_METEO_URL = (
"https://api.open-meteo.com/v1/forecast?latitude=59.9386&longitude=30.3141&current=weather_code&timezone=Asia%2FSingapore")

# =========================================================
# WEATHER CODE → GROUP
# =========================================================

WEATHER_CODE_GROUPS = {
    0: 1,
    1: 2, 2: 2, 3: 2,
    45: 3, 48: 3,
    51: 4, 53: 4, 55: 4,
    56: 5, 57: 5,
    61: 6, 63: 6, 65: 6,
    66: 7, 67: 7,
    71: 8, 73: 8, 75: 8,
    77: 9,
    80: 10, 81: 10, 82: 10,
    85: 11, 86: 11,
    95: 12,
    96: 13, 99: 13,
}

# =========================================================
# PROMPTS (рандом внутри группы)
# =========================================================

WEATHER_PROMPTS = {
    1: [
        "A wide open landscape under a perfectly clear blue sky, bright sunlight, high contrast lighting, cinematic composition, ultra realistic, clear weather, cinematic style",
        "A peaceful landscape under a clear blue sky, soft light, no clouds, gentle color transitions, watercolor painting style, airy and calm atmosphere",
        "An anime-style outdoor scene with a clear blue sky, bright sunlight, vibrant colors, clean outlines, serene mood, anime background art",
    ],
    2: [
        "A realistic landscape with a sky shifting from clear to partly cloudy and overcast, dramatic clouds, soft sunlight, cinematic lighting, realistic weather, cinematic style",
        "A landscape with layered clouds from clear to overcast, soft diffused light, subtle tones, watercolor painting style, atmospheric sky",
        "An isometric outdoor environment with mixed sky conditions, clear areas and overcast clouds, simplified shapes, clean geometry, isometric illustration style",
    ],
    3: [
        "A cold foggy landscape with dense fog, low visibility, rime ice forming on trees, muted colors, moody lighting, cinematic winter atmosphere",
        "A fog-covered winter scene with frost on trees, soft edges, pale colors, dreamy mood, watercolor style weather illustration",
        "An anime-style winter scene with thick fog, icy trees, cold atmosphere, soft lighting, mysterious mood, anime background art",
    ],
    4: [
        "An urban street during drizzle rain, wet asphalt reflecting lights, fine raindrops visible, overcast sky, cinematic rain scene, realistic lighting",
        "A rainy street scene with light to dense drizzle, flowing brush strokes, soft reflections, watercolor painting style, calm rainy mood",
        "An isometric city block with drizzle rain, wet surfaces, small raindrops, simplified buildings, clean isometric illustration",
    ],
    5: [
        "A winter road scene with freezing drizzle, icy surfaces, subtle reflections, cold gray sky, dangerous conditions, cinematic realism",
        "A winter scene with freezing drizzle, icy glaze on trees and ground, soft cold palette, watercolor painting style",
        "An anime-style winter street with freezing drizzle, icy roads, cold atmosphere, muted colors, detailed anime background",
    ],
    6: [
        "A dramatic outdoor scene with rainfall from light to heavy, dark clouds, rain streaks, puddles splashing, cinematic lighting, realistic storm mood",
        "A rainy landscape with varying rain intensity, flowing brush strokes, blurred rain effects, watercolor style illustration",
        "An isometric town scene with rain, visible rain lines, wet ground, simplified geometry, isometric weather illustration",
    ],
    7: [
        "A cityscape during freezing rain, ice-coated trees and power lines, dark sky, reflective surfaces, cinematic winter realism",
        "A frozen city scene with freezing rain, soft icy textures, cool tones, watercolor painting style",
        "An anime-style winter city under freezing rain, icy reflections, cold atmosphere, detailed anime background art",
    ],
    8: [
        "A winter landscape with snowfall from light to heavy, snowflakes visible in the air, snow-covered trees, cinematic lighting, realistic winter scene",
        "A snowy landscape with falling snow, soft white textures, gentle brush strokes, watercolor winter illustration",
        "An isometric winter village with snowfall, snow-covered rooftops, falling snow particles, clean isometric style",
    ],
    9: [
        "A harsh winter scene with snow grains falling, icy particles in the air, low visibility, cold lighting, cinematic realism",
        "A cold weather scene with snow grains, soft grainy texture, pale colors, watercolor style winter weather",
        "An anime-style winter scene with snow grains blowing in the wind, cold atmosphere, subdued colors, anime background art",
    ],
    10: [
        "A powerful rain shower with bursts of heavy rain, strong wind, dark storm clouds, splashing water, cinematic storm scene",
        "A dramatic rain shower scene, expressive brush strokes, dynamic rain motion, watercolor painting style",
        "An isometric landscape with rain showers, varying rain density, simplified storm clouds, isometric illustration style",
    ],
    11: [
        "A winter scene with sudden snow showers, swirling snowflakes, reduced visibility, cold cinematic lighting",
        "A winter landscape with snow showers, soft swirling snow, gentle textures, watercolor illustration",
        "An anime-style snowy scene with intense snow showers, dynamic motion, cold atmosphere, anime background art",
    ],
    12: [
        "A dramatic thunderstorm scene with dark clouds, distant lightning, moderate rain, high contrast lighting, cinematic realism",
        "A stormy sky with lightning and rain, expressive brush strokes, moody colors, watercolor storm illustration",
        "An isometric landscape under a thunderstorm, stylized lightning bolts, dark clouds, isometric style",
    ],
    13: [
        "An intense thunderstorm with hail, ice pellets falling, bright lightning, dark dramatic clouds, cinematic extreme weather scene",
        "A violent storm with hail and lightning, energetic brush strokes, dark tones, watercolor painting style",
        "An anime-style thunderstorm with hail, dramatic lightning, dynamic sky, high-energy anime background art",
    ],
}

# =========================================================
# HELPERS
# =========================================================

def get_weather_code():
    """
    Strictly matches your Open-Meteo response:
    data["current"]["weather_code"]
    """
    try:
        r = requests.get(OPEN_METEO_URL, timeout=10)
        r.raise_for_status()
        data = r.json()
        current = data.get("current")
        if not current:
            return None
        return current.get("weather_code")
    except Exception as e:
        print("Open-Meteo error:", e)
        return None


def pick_prompt(weather_code: int) -> str:
    group = WEATHER_CODE_GROUPS.get(weather_code)
    if not group:
        return "A neutral outdoor scene with calm weather, photo, 35mm"
    return random.choice(WEATHER_PROMPTS[group])

# =========================================================
# HTTP HANDLER
# =========================================================

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self):
        if self.path != "/fw":
            return self._reply(404, b"Not found\n", "text/plain")

        # ---- read & ignore ESP body completely ----
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length:
            self.rfile.read(length)

        # ---- weather logic ----
        weather_code = get_weather_code()
        weather_prompt = pick_prompt(weather_code)

        prompt = weather_prompt
        print(prompt)    

        # ---- Fireworks request ----
        payload = {
            "model": "stable-diffusion-xl-1024-v1-0",
            "prompt": prompt,
            "cfg_scale": 7,
            "width": 640,
            "height": 1536,
            "steps": 5,
            "seed": 0,
            "safety_check": False,
            "output_image_format": "jpeg",
        }

        headers = {
            "Authorization": f"Bearer {FIREWORKS_API_KEY}",
            "Content-Type": "application/json",
            "Accept": "image/jpeg",
            "Accept-Encoding": "identity",
        }

        r = requests.post(
            FW_URL,
            json=payload,
            headers=headers,
            proxies=PROXIES,
            timeout=(15, 600),
        )

        if r.status_code != 200:
            return self._reply(
                r.status_code,
                r.content[:2000],
                r.headers.get("Content-Type", "text/plain"),
            )

        # ---- downscale to 480x320 ----
        img = Image.open(io.BytesIO(r.content)).convert("RGB")
        img = img.resize((320, 480), Image.LANCZOS)

        out = io.BytesIO()
        img.save(out, format="JPEG", quality=90, optimize=True)
        body = out.getvalue()

        # ---- send RAW JPEG ----
        self.wfile.write(b"HTTP/1.1 200 OK\r\n")
        self.wfile.write(b"Content-Type: image/jpeg\r\n")
        self.wfile.write(f"Content-Length: {len(body)}\r\n".encode())
        self.wfile.write(b"Connection: close\r\n\r\n")
        self.wfile.write(body)
        self.wfile.flush()

    def _reply(self, code, body, ctype):
        self.wfile.write(f"HTTP/1.1 {code}\r\n".encode())
        self.wfile.write(f"Content-Type: {ctype}\r\n".encode())
        self.wfile.write(f"Content-Length: {len(body)}\r\n".encode())
        self.wfile.write(b"Connection: close\r\n\r\n")
        self.wfile.write(body)

    def log_message(self, *_):
        pass

# =========================================================
# MAIN
# =========================================================

if __name__ == "__main__":
    print(f"Listening on http://{LISTEN_HOST}:{LISTEN_PORT}/fw")
    ThreadingHTTPServer((LISTEN_HOST, LISTEN_PORT), Handler).serve_forever()
