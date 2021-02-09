import pandas as pd
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import matplotlib.cm as cmx
import sys

fname = 'hdf5_iotest.csv'
if (len(sys.argv) > 1):
    fname = sys.argv[1]

df = pd.read_csv(fname)
wr = df.loc[:,'write-max [s]'].values
rd = df.loc[:,'read-max [s]'].values
cr = df.loc[:,'creat-max [s]'].values
col = df.loc[:,'wall [s]']

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')
ax.scatter(wr, rd, cr, c=col, cmap=cmx.rainbow)
for i in range(len(wr)):
    ax.text(wr[i], rd[i], cr[i], str(i), size=11, zorder=1, color='k')

ax.set_xlabel('write (s)')
ax.set_ylabel('read (s)')
ax.set_zlabel('create (s)')

plt.show()
