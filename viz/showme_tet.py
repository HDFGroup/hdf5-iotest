import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cmx

df = pd.read_csv("hdf5_iotest.csv")
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

s = df['write-max [s]'] + df['read-max [s]'] + df['creat-max [s]']

w = (df['write-max [s]']/s).values
r = (df['read-max [s]']/s).values
cr = (df['creat-max [s]']/s).values

col = df.loc[:,'wall [s]']

ax.scatter(w, r, cr, c=col, cmap=cmx.rainbow)
for i in range(len(w)):
    ax.text(w[i], r[i], cr[i], str(i), size=10, zorder=1, color='k')

ax.set_xlabel('write (s)')
ax.set_ylabel('read (s)')
ax.set_zlabel('creat (s)')

plt.show()
