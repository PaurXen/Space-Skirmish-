import numpy as np
import matplotlib.pyplot as plt

r = 10
cx, cy = 5, 5

pts = [(x+cx, y+cy) for x in range(-r, r + 1)
              for y in range(-r, r + 1)
              if x*x + y*y <= r*r]

ptx = np.array([p[0] for p in pts])
pty = np.array([p[1] for p in pts])

pts2 = {x:[] for x in set(ptx)}
for x, y in pts:
    pts2[x].append(y)
ptsx = set()
for x in sorted(pts2.keys()):
    ptsx.add( (x, min(pts2[x])) )
    ptsx.add( (x, max(pts2[x])) )

pts2 = {y:[] for y in set(pty)}
for x, y in pts:
    pts2[y].append(x)
ptsy = set()
for y in sorted(pts2.keys()):
    ptsy.add( (min(pts2[y]), y) )
    ptsy.add( (max(pts2[y]), y) )

pts = sorted(ptsx.union(ptsy))
print()
print("Number of points:", len(pts))
print(pts)
pts = np.array(pts)



t = np.linspace(0, 2*np.pi, 800)
plt.plot(r*np.cos(t)+cx, r*np.sin(t)+cy)

plt.scatter(pts[:, 0], pts[:, 1], s=25)

plt.gca().set_aspect('equal', adjustable='box')
plt.grid(True)
plt.xlim(-r-1+cx, r+1+cx)
plt.ylim(-r-1+cy, r+1+cy)
plt.title("Integer points inside/on circle (r=10)")
plt.show()

