from struct import *

class Header(object):
   def __init__(self, filename):
      self.filename = filename
      fp = open(filename, "rb")
      self.ver = unpack("3B", fp.read(3))
      if self.ver != (2,1,0):
         raise Exception("Invalid version"), self.ver
      self.dst = unpack("B", fp.read(1))[0]
      self.buildnum = unpack("L", fp.read(4))[0]
      self.time = unpack("d", fp.read(8))[0]
      self.timeadjust = unpack("L", fp.read(4))[0]
      self.imustatus = unpack("L", fp.read(4))[0]
      self.imuvals = unpack("9d", fp.read(9*8))
      self.orthodimensions = unpack("2L", fp.read(2*4))
      self.corners = unpack("4f", fp.read(4*4))
      self.extents = unpack("4L", fp.read(4*4))
      numpoints = (self.extents[1] - self.extents[0] + 2) * (self.extents[3] - self.extents[2] + 2) * 3
      self.points = unpack("%if" % numpoints, fp.read(numpoints*4))
      numlevels = unpack("B", fp.read(1))[0]
      self.leveltable = unpack("%iL" % numlevels, fp.read(numlevels*4))
      self.levelextents = unpack("4L", fp.read(4*4))
      self.ncols = self.levelextents[1] - self.levelextents[0] + 1
      self.nrows = self.levelextents[3] - self.levelextents[2] + 1
      maxnumtiles = self.ncols * self.nrows + 1
      self.leveltiletable = unpack("%iL" % maxnumtiles, fp.read(maxnumtiles*4))

   def __eq__(self,other):
      return self.ncols == other.ncols and self.nrows == other.nrows

   def __repr__(self):
      return """%s
Buildnum: %i
Time: %f + %i
Dimensions: %i, %i (%i, %i)
Tiles: %i
""" % (self.filename,self.buildnum,self.time,self.timeadjust,self.ncols,self.nrows,self.orthodimensions[0],self.orthodimensions[1],len(self.leveltiletable))

def getParts(fname):
   import re
   return re.match("(.*?)([0-9]+)(\\.af)", fname).groups()

def buildFromParts(basename, num, ext):
   return "%s%s%s" % (basename,num,ext)

if __name__ == "__main__":
   import os,sys
   if len(sys.argv) != 2:
      print "%s <dirname>" % sys.argv[0]
      sys.exit(1)
   print "Getting list of files..."
   sys.stdout.flush()
   affiles = os.listdir(sys.argv[1])
   print "Filter..."
   sys.stdout.flush()
   affiles = filter(lambda f: f.endswith('.af'), affiles)
   print "Sort..."
   sys.stdout.flush()
   affiles.sort()
   basename, baseval, ext = getParts(affiles[0])
   idxfile = open("opticks.idx","wb")
   idxfile.write("basename=%s\nfirstframe=%s\next=%s\nnumframes=%i\n" % (basename,baseval,ext,len(affiles)))
   maxwidth,maxheight=0,0
   cnt=0
   print "Begin processing..."
   sys.stdout.flush()
   div = 100.0 / len(affiles)
   for affile in affiles:
      if cnt % 100 == 0:
         print "%i (%i%%)" % (cnt, cnt * div)
         sys.stdout.flush()
      cnt += 1
      h = Header(affile)
      maxwidth=max(maxwidth,h.ncols)
      maxheight=max(maxheight,h.nrows)
   idxfile.write("maxwidth=%i\nmaxheight=%i\n" % (maxwidth,maxheight))
   idxfile.close()
