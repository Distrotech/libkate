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
    try:
      popen=subprocess.Popen(cmdline,stdin=None,stderr=subprocess.PIPE,stdout=subprocess.PIPE,universal_newlines=True,shell=True)
    except OSError,e:
      return None
    if not popen.stdout:
      return None
    if popen.stdin:
      popen.stdin.close()
    if popen.stderr:
      popen.stderr.close()
    line=popen.stdout.readline()
    while line:
      if magic in line:
        popen.stdout.close()
        return command
      line=popen.stdout.readline()
    popen.stdout.close()
    return None

  def probe_command_in(self,commands,options,magic,paths):
    for command in commands:
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
    self.mux_command=self.probe_command_in(['oggz-merge','oggzmerge'],'-v','merge version',external_tools_path)
    self.demux_command=self.probe_command_in(['oggz-rip','oggzrip'],'-v','rip version',external_tools_path)
    self.codecs_command=self.probe_command_in(['oggz-known-codecs'],'-v','oggz-known-codecs version',external_tools_path)
    self.kateenc_command=self.probe_command_in(['kateenc'],'-V','Kate reference encoder - libkate',internal_tools_path)
    self.katedec_command=self.probe_command_in(['katedec'],'-V','Kate reference decoder - libkate',internal_tools_path)
    self.check()

  def run(self,params,stdin):
    if self.on_start!=None: self.on_start()
    try:
      try:
        popen=subprocess.Popen(params,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,universal_newlines=True)
        if stdin==None:
          (stdoutdata,stderrdata)=popen.communicate(stdin)
        else:
          (stdoutdata,stderrdata)=popen.communicate(stdin.encode('utf8'))
        ret=popen.returncode
        # oggz tools can return 0 when they fail, so do not test ret
        msg=stderrdata.encode('utf8')
        if msg!=None and msg!='':
          raise Exception,msg
      except:
        raise
    finally:
      if self.on_end!=None: self.on_end()
  
  def run_demux(self,params,stdin=None):
    if self.demux_command==None:
      raise Exception,'No demux command found'
    self.run([self.demux_command]+params,stdin)

  def run_mux(self,params,stdin=None):
    if self.mux_command==None:
      raise Exception,'No mux command found'
    self.run([self.mux_command]+params,stdin)

  def run_katedec(self,params,stdin=None):
    if self.katedec_command==None:
      raise Exception,'katedec was not found'
    self.run([self.katedec_command]+params,stdin)

  def run_kateenc(self,params,stdin=None):
    if self.kateenc_command==None:
      raise Exception,'kateenc was not found'
    self.run([self.kateenc_command]+params,stdin)

if __name__=='__main__':
  tools=Tools();

