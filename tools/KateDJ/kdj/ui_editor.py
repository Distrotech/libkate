import sys
import wx
from wx.py.editor import EditorNotebook
from wx.py.buffer import Buffer
from wx.py.editor import Editor

class UIEditor(wx.Dialog):
  def __init__(self,parent,filename=None):
    pre=wx.PreDialog()
    pre.Create(parent,wx.ID_ANY,title='Editing Kate streams',pos=(100,100),size=(600,400),style=wx.DEFAULT_DIALOG_STYLE|wx.RESIZE_BORDER)
    self.PostCreate(pre)

    self.notebook=EditorNotebook(self)
    if filename!=None:
      self.addStream(filemame)

    self.Show(True)

  def addStream(self,filename):
     buffer=Buffer()
     panel=wx.Panel(parent=self.notebook,id=-1)
     panel.Bind(wx.EVT_ERASE_BACKGROUND,lambda x: x)
     editor=Editor(parent=panel)
     panel.editor=editor
     sizer=wx.BoxSizer(wx.VERTICAL)
     sizer.Add(editor.window,1,wx.EXPAND)
     panel.SetSizer(sizer)
     panel.SetAutoLayout(True)
     sizer.Layout()
     buffer.addEditor(editor)
     buffer.open(filename)
     self.notebook.AddPage(page=panel,text=buffer.name,select=True)
     editor.setFocus()

