import sys
import os

from tools import Tools
from finder import FindKateStreams

class Muxer:
  def __init__(self,tools,filename,directory,type):
    self.directory=directory
    self.Mux(tools,filename,type)

  def EncodeKateStream(self,tools,kate_stream,type):
    filename=kate_stream['filename']
    params=[]
    params+=['-t',type]
    params+=['-o',filename+'.ogg']
    params+=[filename]
    params+=['-l',kate_stream['language']]
    params+=['-c',kate_stream['category']]
    params+=['-s',kate_stream['serial']]
    tools.run_kateenc(params)

  def Mux(self,tools,filename,type):
    kate_streams=FindKateStreams(self.directory)

    params=[]
    params+=['-o',filename,os.path.join(self.directory,'misc.ogg')]
    for idx in kate_streams:
      kate_stream=kate_streams[idx]
      self.EncodeKateStream(tools,kate_stream,type)
      params+=[kate_stream['filename']+'.ogg']
    tools.run_mux(params)

if __name__=='__main__':
  tools=Tools()
  dir='katedj-tmp-extract-8ELIgg'
  if len(sys.argv)>1:
    dir=sys.argv[1];
  muxer=Muxer(tools,'newdemo.ogg',dir)
