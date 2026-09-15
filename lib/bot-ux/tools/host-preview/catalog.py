"""Package native RGB565 captures as a searchable, fully offline bot catalog.
Run render.sh first, then botux-preview OUTPUT --catalog, then this script OUTPUT.
Requires Pillow; no image generation or browser animation substitutes the firmware.
"""
import base64
import html
import io
import pathlib
import sys
from PIL import Image

source = pathlib.Path(sys.argv[1])
root = pathlib.Path(__file__).resolve().parents[4]
out = root / 'wiki/bot-ux'
(out / 'assets').mkdir(parents=True, exist_ok=True)
notes = {
'Mood': [
'Quiet breath, irregular blinks and the canonical upper-right gaze define the baseline proportions.',
'Wider curious eyes, a slight lean and a gentle nod show attention.',
'A full-body-scale shell of bold depth-shaded points turns through a brisk 1.6-second latitude-delayed breathing cycle.',
'An engaged face pulses with speech. This example enables setTalking(true).',
'Rounded smiling arches, warm cheeks, a buoyant bounce and two large tapered sparkles make delight explicit.',
'A settled, slightly compressed body and lowered, partly closed eyes.',
'Drowsy narrow eyes with extended blinks and a slow 7.2-second breath.',
'Wide rounded eyes and a quick vertical stretch. Also used by poke().',
'A full-body-scale shell of bold sparse points streams upward through a 1.4-second meridian vortex, fading at the poles as it counter-rotates.',
'Patient open eyes, a slight questioning lean and a slow side-to-side search.',
'An exclamation-mark silhouette with a small glitch rhythm signals a blocker.',
'A large rounded check in the lower orb makes completion explicit, paired with a settled glance and two tapered sparkles.',
'Fully closed sleepy eyes, an 8.8-second breath and three drifting, fading z marks.',
'The face explores the complete gaze field, easing through new targets every 0.9–2.8 seconds.'],
'Expression': [
'Uses the current mood’s facial geometry; shown here with Idle.',
'Explicitly restores Idle’s baseline eye proportions even over resting moods.',
'Wide attentive eyes, a gentle lean and an upper-right glance.',
'Calmer narrow eyes with balanced spacing and a centered gaze.',
'Continuously morphs the eyes into smooth smiling arches.',
'A subtle opposing lean, asymmetric eyes and a sideward glance.',
'A shy lowered glance with a small rotation of the pair.',
'Closes one eye through the same eased capsule geometry.',
'A slow twisting eye pair suggests disorientation without replacing the glyph.',
'Wider, rounder eye marks create an alert face.'],
'Animation': [
'Chooses choreography from the effective mood: calm, curious, orbit, bounce, sparkle or glitch.',
'Gentle drifting, breathing and small eye rotation.',
'An inquisitive lean with small horizontal and vertical movement.',
'A smooth orbital body path with coordinated eye movement.',
'A lively vertical bounce with squash and stretch.',
'A bounded brief jitter, suitable for the blocked lifecycle.',
'A flowing sideways sway and eye rotation.',
'Two large tapered sparkles twinkle near the upper silhouette without crossing the face.']}
zh_notes = {
'Mood': ['轻柔呼吸、不规则眨眼与固定的右上视线，定义所有表情的基础比例。','睁大好奇的双眼，轻微倾身、点头，表示正在认真倾听。','与常规身体同等视觉尺度的醒目纵深点阵，以 1.6 秒基础周期旋转呼吸，纬度波动依次传递。','说话时身体随语音脉动；示例已开启 setTalking(true)。','圆润笑弧眼、温暖腮红、轻快弹跳与两枚醒目的收尖星芒，清楚表达喜悦。','身体下沉并略微压扁，半闭的眼睛低垂。','困倦的细眼、较长眨眼，以及 7.2 秒周期的慢呼吸。','更圆、更大的双眼与快速竖向拉伸，也用于 poke() 的惊讶阶段。','与常规身体同等视觉尺度的醒目稀疏点阵，以 1.4 秒基础周期沿经线上升，在两极淡出并反向旋转。','保持清醒的耐心神态，略微歪头，缓慢左右张望。','感叹号替代身体轮廓，轻微抖动提示工作受阻。','身体下半部的大号圆角勾号明确表示完成，并搭配沉稳侧目与两枚收尖星芒。','完全闭眼、8.8 秒深呼吸，以及三枚逐渐飘起并淡出的 z 字。','脸部探索完整视线范围，每隔 0.9–2.8 秒平滑转向新目标。'],
'Expression': ['跟随当前心情的脸部形态；此处显示空闲状态。','显式恢复空闲状态的基础眼形，覆盖困倦等心情的闭眼形态。','睁大双眼、轻轻歪头，并看向右上方。','收窄双眼、保持对称间距与居中视线。','双眼通过连续形变变成平滑的笑弧。','轻微反向倾斜、不对称眼形与侧目。','略微旋转眼睛，害羞地向下看。','一只眼睛沿同一套平滑几何逐渐闭合。','双眼缓慢扭转以表示眩晕，不替换成独立符号。','更宽、更圆的双眼构成警觉神态。'],
'Animation': ['按有效心情选择平静、好奇、环绕、弹跳、闪耀或闪动节奏。','轻柔漂移、呼吸和小幅眼睛旋转。','带有好奇感的侧倾，以及细微的水平和竖直移动。','平滑的环绕路径，配合眼睛移动。','明显的竖直弹跳，伴随身体压缩和拉伸。','短促、幅度有限的抖动，适合受阻状态。','流畅的左右摇摆与眼睛旋转。','两枚醒目的收尖星芒在身体上方轮廓附近错相闪动，不穿过脸部。']}
records=[]
for line in (source/'catalog.tsv').read_text().splitlines():
    ident,group,name,zh=line.split('\t'); index=int(ident.split('-')[1])
    raw=(source/(ident+'.rgb')).read_bytes(); stride=120*120*3
    frames=[Image.frombytes('RGB',(120,120),raw[i:i+stride]) for i in range(0,len(raw),stride)]
    gif=io.BytesIO(); frames[0].save(gif,format='GIF',save_all=True,append_images=frames[1:],duration=100,loop=0,optimize=True)
    (out/'assets'/(ident+'.gif')).write_bytes(gif.getvalue())
    png=io.BytesIO();frames[0].save(png,format='PNG')
    records.append((ident,group,name,zh,zh_notes[group][index]+" "+notes[group][index],base64.b64encode(gif.getvalue()).decode(),base64.b64encode(png.getvalue()).decode()))
header='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>BotUx · A living field guide</title>
<style>:root{color-scheme:dark;font:16px/1.6 system-ui;background:#0a0e14;color:#e8edf3}*{box-sizing:border-box}body{margin:0}main{max-width:1140px;margin:auto;padding:48px 24px}header{max-width:760px;margin-bottom:32px}h1{font-size:clamp(34px,6vw,64px);line-height:1.05;letter-spacing:-.04em;margin:12px 0 24px}h2{margin:6px 0;font-size:21px}p{color:#b4c2d2}a{color:#8fc4ff}.eyebrow{color:#8fc4ff;letter-spacing:.12em;text-transform:uppercase;font-size:12px}.controls{display:flex;flex-wrap:wrap;gap:12px;padding:16px 0;position:sticky;top:0;background:#0a0e14ee;z-index:1}input,select,button{font:inherit;background:#17212d;color:#e8edf3;border:1px solid #405268;border-radius:10px;padding:10px 12px}input{flex:1;min-width:160px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:18px}article{background:#111923;border:1px solid #273648;border-radius:20px;padding:20px;scroll-margin-top:100px}article img{display:block;width:120px;max-width:100%;height:auto;image-rendering:auto;margin:0 auto 16px;border-radius:16px}article p{font-size:14px;margin:10px 0}code{font-size:12px;color:#8fc4ff}article[hidden]{display:none}.muted{font-size:13px;color:#91a4b8}.counts{margin:16px 0}footer{margin-top:40px;border-top:1px solid #273648;padding-top:24px}@media(max-width:500px){main{padding:28px 16px}.grid{grid-template-columns:1fr}.controls{position:static}article{padding:18px}}@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}}</style>
<main><header><div class="eyebrow">BotUx / Native renderer / 2026-09-16</div><h1>小小的脸，<br>完整的情绪语言。</h1><p>浏览全部已实现的心情、表情和动画。每个示例由真实 C++ 渲染器生成，长 7.2 秒、每秒 10 帧；支持中英文检索。 Explore every implemented mood, expression and animation. These 7.2-second examples capture the actual C++ component at 10 frames per second. Search English or Chinese names, descriptions or API values.</p><p class="muted">14 种心情 · 10 种表情 · 8 种动画 · 1,120 种组合。心情决定状态，表情选择眼形，动画控制节奏；Auto 跟随心情。 14 moods · 10 expressions · 8 animations · 1,120 combinations. Mood sets the state, expression selects the face, and animation chooses its movement. Auto follows the mood. All examples use the default Round body and Oval eyes.</p></header>
<div class="controls"><input id="search" type="search" aria-label="Search catalog" placeholder="搜索：熟睡、等待、眨眼、blink…"><select id="group" aria-label="Filter category"><option value="">全部分类</option><option value="Mood">心情 Mood</option><option value="Expression">表情 Expression</option><option value="Animation">动画 Animation</option></select><button id="motion">暂停动画</button></div><p id="count" class="counts" aria-live="polite"></p><section class="grid" aria-label="Bot vocabulary">'''
cards=[]
for ident,group,name,zh,note,gif,png in records:
    search=html.escape(f'{group} {name} {zh} {note}'.lower(),quote=True)
    api_name='LookingAround' if name == 'Looking around' else name.replace(' ', '')
    cards.append(f'<article id="{ident}" data-group="{group}" data-search="{search}"><img width="120" height="120" alt="{name} {group.lower()} rendered by BotUx" src="data:image/gif;base64,{gif}" data-still="data:image/png;base64,{png}"><div class="eyebrow">{group} · {zh}</div><h2>{name}</h2><code>{group}::{api_name}</code><p>{note}</p></article>')
footer='''</section><footer><h2>如何组合使用</h2><p>心情定义持续状态，表情覆盖眼睛形态，动画选择运动节奏。Thinking、Working 与 Blocked 会替换身体轮廓；前两者使用与常规 Bot 身体同等视觉尺度、更醒目的纵深点阵，基础周期分别为 1.6 秒和 1.4 秒。StopWatch 二级速度与强度会把实际周期调节为约 4.0 秒和 3.5 秒。LookingAround 探索完整视线范围；Idle 保持右上视线。FaceSide 可固定左右镜像，也可跟随视线自动切换。暂停按钮切换为静帧；系统开启减少动态效果时默认暂停。</p><p>Set a persistent mood with <code>setMood()</code>, choose an optional face with <code>setExpression()</code>, and a rhythm with <code>setAnimation()</code>. Thinking and Working replace the avatar with bold, depth-shaded point shells matching the regular Bot body's visual scale, with 1.6- and 1.4-second base periods; the StopWatch level-two speed and intensity produce about 4.0- and 3.5-second cycles. Blocked uses an exclamation. LookingAround explores the complete gaze field while Idle holds the canonical upper-right pose. <code>setFaceSide()</code> selects automatic, left or right mirroring.</p><p>Nine explicit gaze directions add continuous eye proportion changes. Temporary <code>gazeAt()</code> applies while Idle or LookingAround; poke briefly runs Surprise → Happy. Speed, motion amount, reduced motion, IMU tilt, names, bilingual descriptions, battery, signal and optional clock/label overlays remain host-controlled.</p><p class="muted">这些是原生主机渲染图，非设备录像。眼睛、圆形身体和球形点阵采用组件自身的 RGB565 抗锯齿；其他 M5GFX 图形使用主机近似。GIF 量化及截取片段循环可能产生实时固件没有的边界跳变。此页面嵌入全部资源，可离线使用；不代表实机性能验证。 Evidence: these are host RGB565 captures of BotUx.cpp, not device recordings. Eye, ellipse body and orb-point capsule boundaries use the component’s own coverage renderer. Other native M5GFX shapes/fonts are approximations. GIF color quantization and the repeating 7.2-second sampling window may introduce visible loop boundaries absent from the live renderer. No hardware frame-time or optical acceptance is implied. Pause replaces examples with static first frames; reduced-motion preference pauses initially. This HTML embeds every asset and works offline.</p><a href="intro.md">文档与复现方法 / Source guide</a></footer></main>
<script>const cards=[...document.querySelectorAll('article')],search=document.querySelector('#search'),group=document.querySelector('#group'),count=document.querySelector('#count'),motion=document.querySelector('#motion');let paused=matchMedia('(prefers-reduced-motion: reduce)').matches;cards.forEach(c=>{const i=c.querySelector('img');i.dataset.animated=i.src});function filter(){let n=0;cards.forEach(c=>{c.hidden=!!((group.value&&c.dataset.group!==group.value)||!c.dataset.search.includes(search.value.trim().toLowerCase()));if(!c.hidden)n++});count.textContent='显示 '+n+' / '+cards.length+' 项'}function renderMotion(){cards.forEach(c=>{let i=c.querySelector('img');i.src=paused?i.dataset.still:i.dataset.animated});motion.textContent=paused?'播放动画':'暂停动画';motion.setAttribute('aria-pressed',String(paused))}search.addEventListener('input',filter);group.addEventListener('change',filter);motion.onclick=()=>{paused=!paused;renderMotion()};filter();renderMotion();</script></html>'''
def save_png_when_pixels_change(image, path):
    if path.exists():
        with Image.open(path) as current:
            if current.size == image.size and current.convert('RGBA').tobytes() == image.convert('RGBA').tobytes():
                return
    image.save(path)

gaze = Image.open(source / 'gaze-directions.svg.ppm')
gaze_path = root / 'lib/bot-ux/docs/nine-directions-raster.png'
save_png_when_pixels_change(gaze, gaze_path)
save_png_when_pixels_change(Image.open(source / 'idle-up-right.ppm'), root / 'lib/bot-ux/docs/idle-up-right-raster.png')
save_png_when_pixels_change(Image.open(source / 'happy-done-themes.ppm'), root / 'lib/bot-ux/docs/happy-done-raster.png')
save_png_when_pixels_change(Image.open(source / 'happy-done-small.ppm'), root / 'lib/bot-ux/docs/happy-done-small-raster.png')
gaze_data = base64.b64encode(gaze_path.read_bytes()).decode()
footer = footer.replace('<footer>', '<footer><h2>九方向眼神 · Gaze perspective</h2><p>左上 / 上 / 右上，左 / 正面 / 右，左下 / 下 / 右下。由相同原生渲染器直接输出；下看的极限位置更接近中心，左右方向保持镜像透视。</p><img width="600" height="600" style="display:block;width:100%;max-width:600px;height:auto" alt="Nine gaze directions: up-left, up, up-right; left, center, right; down-left, down, down-right" src="data:image/png;base64,' + gaze_data + '">')
(out/'intro.html').write_text(header+'\n'.join(cards)+footer)
md=['# BotUx illustrated field guide\n','[Open the searchable offline catalog](intro.html). All 32 entries have actual-source animated examples. The standalone HTML embeds its images; Markdown uses the adjacent assets.\n','The shared component renders into a caller-owned M5Canvas. Mood, expression and animation form 1,120 combinations (14 × 10 × 8). Existing enum values are preserved; Asleep is mood 12 and LookingAround is appended as mood 13.\n']
for group in notes:
    md.append(f'## {group}s\n')
    for ident,g,name,zh,note,gif,png in records:
        if g==group:
            api_name='LookingAround' if name == 'Looking around' else name.replace(' ', '')
            md.append(f'### {name} · {zh}\n\n![{name} animation](assets/{ident}.gif)\n\n`{group}::{api_name}` — {note}\n')
md.append('''## 九方向眼神 · Gaze perspective

![九方向原生视线图](../../lib/bot-ux/docs/nine-directions-raster.png)

上排：左上 / 上 / 右上。中排：左 / 正面 / 右。下排：左下 / 下 / 右下。下看的极限位置更接近中心；Auto 是额外的自主视线模式。

## Composition and controls

Auto follows the effective mood. Thinking and Working replace the avatar with larger depth-shaded point shells; Blocked replaces it with an exclamation. Thinking uses a 1.6-second latitude-delayed base cycle, while Working streams upward through a 1.4-second pole-faded vortex. StopWatch level-two speed and intensity produce approximately 4.0- and 3.5-second cycles. Explicit Neutral cannot restore eyes over these silhouette states. LookingAround explores the complete gaze field; Idle holds the canonical upper-right pose. `setFaceSide()` selects automatic, left or right mirroring.

`setAnimationSpeed(0.25..3)`, `setMotionAmount(0..2)` and `setReducedMotion()` apply to choreography. Zero amount freezes shell and z-mark motion. `gazeAt()` temporarily overrides gaze in Idle or LookingAround. `poke()` runs surprise → happy → prior mood. `setTalking()` controls speech pulsing. Hosts can supply IMU tilt/shake, battery, signal, time, labels, names and bilingual descriptions. Eight curated presets remain compatible.

## Reproduce and validate

From the repository root:

```sh
lib/bot-ux/tools/host-preview/render.sh
./tmp/botux-preview/botux-preview tmp/botux-preview --catalog
python3 lib/bot-ux/tools/host-preview/catalog.py tmp/botux-preview
```

Packaging requires Pillow. The native C++ renderer emits 72 frames per entry at 120×120, 100 ms apart, using fixed blink seed 1234. The packager encodes GIF assets and embeds them into the searchable HTML. Sources are `lib/bot-ux/src/BotUx.{h,cpp}` and `lib/bot-ux/tools/host-preview/`.

The focused harness verifies coverage, pose direction, mirroring, reduced/zero motion, transition continuity, both orb phase wraps and sleep-loop continuity. These are native host renders, not hardware captures. Component AA ellipse/capsule rendering is real; other M5GFX shapes/fonts remain approximations. GIF quantization and a finite repeating capture can introduce sampling or loop artifacts absent from continuous firmware. No device performance claim is made.
''')
(out/'intro.md').write_text('\n'.join(md))
print(f'Wrote {len(records)} entries to {out}, HTML {(out/"intro.html").stat().st_size:,} bytes')
