#include "include-general.h"
#include "windparse.h"
#include "BoatData.h"

/*
figure out a way to determine if the boat has tacked or jibed
maybe trailing average of AWA; if it is + and goes - or vice versa
then compare TWD before and after tack/jibe as a "sanity check" on instruments

Using apparent wind angle alone, a sign change (from negative to positive or vice versa) is a good basic indicator of a tack or gybe, but it needs some filtering and extra conditions to be reliable.[1][2]

## Your moving-average idea

Your proposal: keep a moving average of AWA; when the current AWA has the opposite sign to the average, flag a tack/gybe. That helps reject brief swings due to waves or steering noise and is a reasonable core approach.  

Refinements to make it robust:  
- Require |AWA| to cross a threshold away from the no‑sail zone or dead‑downwind (e.g. |AWA| > 30° upwind, |AWA| > 120° downwind) both before and after the maneuver.  
- Require the sign change to persist for some minimum time (e.g. 3–5 samples or 2–3 seconds) before declaring a completed tack/gybe.  
- Optionally use a **median filter** or low‑pass filter instead of a straight mean to better reject spikes.[3]

## Other signals you can combine

More robust instruments usually combine several cues:  

- **AWA sign change + boat heading change**: If you also have compass or COG, you can demand that heading has rotated ~80–120° (tack) or ~60–160° (gybe) in the same window as the AWA sign change. This greatly reduces false positives from big oscillations in wind or steering.[4][5]
- **TWA instead of AWA**: Using TWA smooths out some boat‑speed‑related noise; a sign change in TWA (port ↔ starboard) with magnitude above a threshold is a very solid indicator of tack/gybe, but you need true‑wind calculation first.[6][1]
- **Speed/heel pattern**: Many boats momentarily slow and change heel in a characteristic way during a tack; some racing instruments use a short time window of speed/heel drop then recovery in combination with wind‑angle sign change to detect “maneuver complete.”[7][8]

## Practical recommendation

- For a simple, on‑board algorithm with AWA + STW only, your **moving‑average sign‑change** plus a few hard thresholds (minimum |AWA| before/after, minimum time) is a sensible, lightweight solution.  
- If you already have heading (from compass or RTK) and possibly TWA, combine **wind‑angle sign change + heading rotation** for a more race‑grade, reliable tack/gybe detector that will be less sensitive to gusts and steering noise.[9][1]

[1](https://support.raymarine.com/s/article/Apparent-Wind--True-Wind-and-Ground-Wind--and-data-required-to-calculate-them)
[2](https://en.wikipedia.org/wiki/Apparent_wind)
[3](https://orc.org/uploads/files/Rules-Regulations/2025/Speed-Guide-Explanation-2025.pdf)
[4](https://sailing-blog.nauticed.org/tacking-and-gybing-maneuvers/)
[5](https://harborsailboats.com/tacking-and-gybing-made-easy/)
[6](https://www.bwsailing.com/cc/2017/05/calculating-the-true-wind-and-why-it-matters/)
[7](https://vakaros.com/blogs/news/vmg-and-tack-loss-new-training-tools)
[8](https://evolution-tactic.com/en/discover/)
[9](https://www.bwsailing.com/bw/true-wind-from-apparent-wind/)
[10](https://www.cal-sailing.org/blogfrontpage/recent-blog-posts/entry/demystifying-apparent-wind-part-1)
[11](https://www.spinnakersailing.com/apparent-wind/)
[12](https://sailzing.com/apparent-wind-clues/)
[13](https://sailing-blog.nauticed.org/americas-cup-apparent-wind/)
[14](https://easysea.org/blogs/utility/https-www-easysea-org-blog-vmg-explained-sailing-smarter)
[15](https://forums.sailboatowners.com/threads/using-vmg-information-for-trimming-and-tacking.68332/)
[16](https://www.laketahoesail.com/post/what-is-vmg-velocity-made-good)
[17](https://www.yachtingmonthly.com/sailing-skills/apparent-wind-how-to-predict-it-and-use-it-to-your-advantage-87841)
[18](https://www.racetac.com/blog_2.htm)
[19](https://www.wottac.com)
[20](https://www.reddit.com/r/sailing/comments/1cxe37h/whats_your_trick_to_find_the_wind/)
[21](https://www.davisinstruments.com/pages/what-is-a-sailboat-tacking-indicator)
[22](https://www.facebook.com/groups/DragonFlite95/posts/888138501365031/)
*/

#if 0
static double maxTWS;

// Simplified function - calculations moved to client-side JavaScript
// Only processes raw sensor data, no derived calculations
void processRawSensorData() {
  // This function can be used for any raw sensor processing
  // that needs to happen on the ESP32 side before streaming
  // Currently just updates maxTWS tracking if needed
  if (pBD->AWS > maxTWS) {
    maxTWS = pBD->AWS; // Track max apparent wind for reference
  }
}
#endif
