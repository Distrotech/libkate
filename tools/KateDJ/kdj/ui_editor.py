import sys
import wx
from wx.py.editor import EditorNotebook
from wx.py.buffer import Buffer
from wx.py.editor import Editor
from tester import Tester

class UIEditor(wx.Dialog):
  def __init__(self,parent,tools,filename=None):
    pre=wx.PreDialog()
    pre.Create(parent,wx.ID_ANY,title='Editing Kate streams',pos=(100,100),size=(600,400),style=wx.DEFAULT_DIALOG_STYLE|wx.RESIZE_BORDER)
    self.PostCreate(pre)

    self.tools=tools

    box=wx.BoxSizer(wx.VERTICAL)
    self.SetSizer(box)

    self.notebook=EditorNotebook(self)
    box.Add(self.notebook,1,wx.EXPAND)
    self.notebook.Bind(wx.EVT_NOTEBOOK_PAGE_CHANGED,self.OnPageChanged)
    if filename!=None:
      self.addStream(filemame)

    buttons=wx.BoxSizer(wx.HORIZONTAL)
    box.Add(buttons)

    test=wx.Button(self,wx.ID_ANY,'Test')
    test.Bind(wx.EVT_BUTTON,self.OnTest)
    buttons.Add(test)

    save=wx.Button(self,wx.ID_SAVE,'Save')
    save.Bind(wx.EVT_BUTTON,self.OnSave)
    buttons.Add(save)

    save_all=wx.Button(self,wx.ID_SAVE,'Save all')
    save_all.Bind(wx.EVT_BUTTON,self.OnSaveAll)
    buttons.Add(save_all)

    quit=wx.Button(self,wx.ID_OK,'Quit')
    quit.Bind(wx.EVT_BUTTON,self.OnQuit)
    buttons.Add(quit)

    self.buffers=[]

    self.Layout()
    self.Show(True)

  def addStream(self,filename):
     buffer=Buffer()
     panel=wx.Panel(parent=self.notebook,id=-1)
     panel.Bind(wx.EVT_ERASE_BACKGROUND,lambda x: x)
     editor=Editor(parent=panel)

     panel.editor=editor
     panel.buffer=buffer
     panel.filename=filename

     sizer=wx.BoxSizer(wx.VERTICAL)
     sizer.Add(editor.window,1,wx.EXPAND)
     panel.SetSizer(sizer)
     panel.SetAutoLayout(True)
     sizer.Layout()
     buffer.addEditor(editor)
     buffer.open(filename)
     self.notebook.AddPage(page=panel,text=buffer.name,select=True)
     self.buffers.append(buffer)
     editor.setFocus()

  def OnPageChanged(self,event):
    new=event.GetSelection()
    window=self.notebook.GetPage(new)
    self.current_panel=window
    event.Skip()

  def OnTest(self,event):
    try:
      tester=Tester(self.tools,self.current_panel.editor.getText(),'kate')
      wx.MessageBox('No errors found','Success',parent=self,style=wx.OK)
    except Exception,e:
      wx.MessageBox('Error:\n'+str(e),'Test failed',parent=self,style=wx.OK)

  def OnSave(self,event):
    self.current_panel.buffer.save()
    event.Skip()

  def OnSaveAll(self,event):
    for buffer in self.buffers:
      buffer.save()
    event.Skip()

  def getNumChanged(self):
    changed=0
    for buffer in self.buffers:
      if buffer.hasChanged():
        changed+=1
    return changed

  def OnQuit(self,event):
    changed=self.getNumChanged()
    quit=True
    if changed>0:
      dlg=wx.MessageDialog(self,'There are unsaved changes. Quit anyway ?','Unsaved changes')
      if dlg.ShowModal()!=wx.ID_OK:
        quit=False
      dlg.Destroy()
    if quit:
      event.Skip()

