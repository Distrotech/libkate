import sys
import os

from tools import Tools

class Tester:
  def __init__(self,tools,contents,type):
    self.Test(tools,contents,type)

  def Test(self,tools,contents,type):
    params=[]
    params+=['-t','kate']
    params+=['-o','-']
    params+=['-l','x-test','-c','test']
    tools.run_kateenc(params,stdin=contents)

