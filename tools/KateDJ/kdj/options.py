import os

class Options:
  def __init__(self,path):
    self.rc=os.path.join(path,'.KateDJrc')
    #print 'options: config file is '+self.rc
    try:
      self.SetDefaults()
      self.Load()
    except:
      pass

  def SetDefaults(self):
    self.save_as_copy=True
    self.remove_temporary_files=False
    self.format='kate'

  def Load(self):
    f=open(self.rc,'Ur')
    if not f:
      return
    try:
      try:
        line=f.readline()
        while line:
          line=self.Frob(line)
          self.Parse(line)
          line=f.readline()
      except:
        pass
    finally:
      f.close()
    
  def Save(self):
    f=open(self.rc,'w')
    f.write('save_as_copy='+self.Bool2String(self.save_as_copy)+'\n')
    f.write('remove_temporary_files='+self.Bool2String(self.remove_temporary_files)+'\n')
    f.write('format='+self.format.lower()+'\n')
    f.close()

  def Bool2String(self,val):
    if val:
      return 'true'
    else:
      return 'false'

  def String2Bool(self,val):
    val=val.lower()
    if val=='true' or val=='1' or val=='yes' or val=='y':
      return True
    else:
      return False

  def String2Format(self,val):
    val=val.lower()
    if val=='kate':
      return 'kate'
    if val=='srt':
      return 'srt'
    if val=='lrc':
      return 'lrc'
    return 'kate'

  def Frob(self,line):
    line=line.split('#')[0]
    line=line.split('\n')[0]
    return line

  def Parse(self,line):
    var,val=line.split('=')[0:2]
    if var.lower()=='save_as_copy': self.save_as_copy=self.String2Bool(val)
    elif var.lower()=='remove_temporary_files': self.remove_temporary_files=self.String2Bool(val)
    elif var.lower()=='format': self.format=self.String2Format(val)
    #else: print 'unknown variable: '+line

