#!/usr/bin/env python

import sys
import os
import subprocess

external_tools_path=['','/usr/bin/','/usr/local/bin/']
internal_tools_path=['../']+external_tools_path

class Tools:
  demux_command=None
  mux_command=None
  codecs_command=None
  kateenc_command=None
  katedec_command=None

  def __init__(self,on_start=None,on_end=None):
    self.on_start=on_start
    self.on_end=on_end
    self.find_tools()

  def probe_command(self,command,options,magic):
    cmdline=command+' '+options
    fin,f,ferr=os.popen3(cmdline)
    if not f:
      return None
    if fin:
      fin.close()
    if ferr:
      ferr.close()
    line=f.readline()
    while line:
      if magic in line:
        f.close()
        return command
      line=f.readline()
    f.close()
    return None

  def probe_command_in(self,command,options,magic,paths):
    for path in paths:
      found=self.probe_command(path+command,options,magic)
      if found!=None:
        return found
    return None

  def check(self):
    msg=''
    if self.demux_command==None:
      msg+='Failed to find the oggz-rip demuxing tool\n'

    if self.mux_command==None:
      msg+='Failed to find the oggz-merge muxing tool\n'

    if self.kateenc_command==None:
      msg+='Failed to find kateenc\n'

    if self.katedec_command==None:
      msg+='Failed to find katedec\n'

    # oggz-known-codecs is ok not to be found

    if msg!='':
      raise Exception,msg

  def find_tools(self):
    self.mux_command=self.probe_command_in('oggz-merge','-v','oggz-merge version',external_tools_path)
    self.demux_command=self.probe_command_in('oggz-rip','-v','oggz-rip version',external_tools_path)
    self.codecs_command=self.probe_command_in('oggz-known-codecs','-v','oggz-known-codecs version',external_tools_path)
    self.kateenc_command=self.probe_command_in('kateenc','-V','Kate reference encoder - libkate',internal_tools_path)
    self.katedec_command=self.probe_command_in('katedec','-V','Kate reference decoder - libkate',internal_tools_path)
    self.check()

  def run(self,params):
    if self.on_start!=None: self.on_start()
    try:
      try:
        popen=subprocess.Popen(params,stderr=subprocess.PIPE,universal_newlines=True)
        popen.wait()
        ret=popen.returncode
        # oggz tools can return 0 when they fail, so do not test ret
        msg=''
        line=popen.stderr.readline()
        while line:
          msg+=line
          line=popen.stderr.readline()
        if msg!='':
          raise Exception,msg
      except:
        raise
    finally:
      if self.on_end!=None: self.on_end()
  
  def run_demux(self,params):
    if self.demux_command==None:
      raise Exception,'No demux command found'
    self.run([self.demux_command]+params)

  def run_mux(self,params):
    if self.mux_command==None:
      raise Exception,'No mux command found'
    self.run([self.mux_command]+params)

  def run_katedec(self,params):
    if self.katedec_command==None:
      raise Exception,'katedec was not found'
    self.run([self.katedec_command]+params)

  def run_kateenc(self,params):
    if self.kateenc_command==None:
      raise Exception,'kateenc was not found'
    self.run([self.kateenc_command]+params)

if __name__=='__main__':
  tools=Tools();

