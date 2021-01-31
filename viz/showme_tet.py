import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.cm as cmx
import sys

fname = 'hdf5_iotest.csv'
if (len(sys.argv) > 1):
    fname = sys.argv[1]

df = pd.read_csv(fname)
ss = df['write-max [s]'] + df['read-max [s]'] + df['creat-max [s]']
wr = (df['write-max [s]']/ss).values
rd = (df['read-max [s]']/ss).values
cr = (df['creat-max [s]']/ss).values
col = df.loc[:,'wall [s]']

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
ax.scatter(wr, rd, cr, c=col, cmap=cmx.rainbow)
for i in range(len(wr)):
    ax.text(wr[i], rd[i], cr[i], str(i), size=10, zorder=1, color='k')

ax.set_xlabel('write (s)')
ax.set_ylabel('read (s)')
ax.set_zlabel('creat (s)')

# show the reference triangle outline
ax.plot([1.0, 0.0, 0.0, 1.0], [0.0, 1.0, 0.0, 0.0], zs=[0.0, 0.0, 1.0, 0.0])

plt.show()
