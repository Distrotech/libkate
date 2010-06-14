import sys
import os

from tools import Tools

class Tester:
  def __init__(self,tools,contents,format):
    self.Test(tools,contents,format)

  def Test(self,tools,contents,format):
    params=[]
    params+=['-t',format]
    params+=['-o','-']
    params+=['-l','x-test','-c','test']
    tools.run_kateenc(params,stdin=contents)

