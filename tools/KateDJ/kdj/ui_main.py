import sys
import os
import wx
from ui_editor import UIEditor
from ui_options import UIOptions

from constants import *
from tools import Tools
from demuxer import Demuxer
from muxer import Muxer
from finder import FindKateStreams
from options import Options

base_width=480
base_height=240
serial_width=(base_width/4)
language_width=(base_width/4)
category_width=(base_width/2)
padding=8
button_width=(base_width-padding*2)/3
button_height=32
list_height=96

class UIMain(wx.Frame):
  def __init__(self):
    wx.Frame.__init__(self,None,wx.ID_ANY,kdj_name_version)

    self.options=Options(wx.StandardPaths.Get().GetUserConfigDir())

    try:
      self.tools=Tools(wx.BeginBusyCursor,wx.EndBusyCursor)
    except Exception,e:
      wx.MessageBox('Failed to find necessary tools:\n'+str(e),'Error',style=wx.OK|wx.CENTRE|wx.ICON_ERROR)
      sys.exit(1)

    self.filename=None
    self.demuxer=None

    vbox=wx.BoxSizer(wx.VERTICAL)
    self.SetSizer(vbox)

    self.action_button=self.AddActionButton()
    vbox.Add(self.action_button,0,wx.EXPAND|wx.ALL,border=padding)

    self.list=self.AddList()
    vbox.Add(self.list,1,wx.EXPAND|wx.ALL,border=padding)

    bbox=wx.BoxSizer(wx.HORIZONTAL)
    vbox.Add(bbox,0,wx.EXPAND|wx.ALL,border=padding)
    self.options_button=self.AddOptionsButton()
    bbox.Add(self.options_button,0,wx.EXPAND)
    bbox.Add((padding,0),1,wx.EXPAND)
    self.help_button=self.AddHelpButton()
    bbox.Add(self.help_button,0,wx.EXPAND)
    self.quit_button=self.AddQuitButton()
    bbox.Add((padding,0),1,wx.EXPAND)
    bbox.Add(self.quit_button,0,wx.EXPAND)

    self.SetMinSize((base_width+2*padding,base_height))
    self.Fit()
    self.Show(True)

  def SetupActionButton(self,button):
    if self.filename==None:
      button.SetLabel('Load Ogg stream')
      button.Bind(wx.EVT_BUTTON,self.OnLoadButton,button)
    elif self.demuxer!=None:
      button.SetLabel('Remux file from parts')
      button.Bind(wx.EVT_BUTTON,self.OnMuxButton,button)
    else:
      button.SetLabel('Demux file')
      button.Bind(wx.EVT_BUTTON,self.OnDemuxButton,button)

  def AddButton(self,text):
    button=wx.Button(self,wx.ID_ANY,text)
    return button

  def AddLoadButton(self):
    button=self.AddButton('Load file')
    button.Bind(wx.EVT_BUTTON,self.OnLoadButton,button)
    return button

  def AddActionButton(self):
    button=self.AddButton('')
    self.SetupActionButton(button)
    return button

  def AddOptionsButton(self):
    button=wx.Button(self,wx.ID_ANY,'Options',size=(button_width,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnOptionsButton,button)
    return button

  def AddHelpButton(self):
    button=wx.Button(self,wx.ID_HELP,'Help',size=(button_width,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnHelpButton,button)
    return button

  def AddQuitButton(self):
    button=wx.Button(self,wx.ID_EXIT,'Quit',size=(button_width,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnQuitButton,button)
    return button

  def OnListSizeChanged(self,event):
    self.Refresh()
    event.Skip()

  def Edit(self,list_idx=-1):
    wx.BeginBusyCursor()
    dlg=UIEditor(self,self.tools,format=self.options.format)
    try:
      kate_streams=FindKateStreams(self.demuxer.GetDirectory())
      if list_idx>=0:
        idx=self.list.GetItemData(list_idx)
        dlg.addStream(kate_streams[str(idx)]['filename'])
      else:
        for idx2 in kate_streams:
          dlg.addStream(kate_streams[idx2]['filename'])
      dlg.ShowModal()
    except Exception,e:
      print 'Exception: %s' % str(e)
    dlg.Destroy()
    wx.EndBusyCursor()

  def OnListDoubleClicked(self,event):
    idx=self.list.GetFirstSelected()
    if idx>=0:
      self.Edit(idx)
    event.Skip()

  def AddList(self):
    list=wx.ListCtrl(self,style=wx.LC_REPORT)
    list.ClearAll()
    list.InsertColumn(0,'Serial')
    list.InsertColumn(1,'Language')
    list.InsertColumn(2,'Category')
    list.SetColumnWidth(0,serial_width)
    list.SetColumnWidth(1,language_width)
    list.SetColumnWidth(2,category_width)
    list.Bind(wx.EVT_SIZE,self.OnListSizeChanged)
    list.Bind(wx.EVT_LEFT_DCLICK,self.OnListDoubleClicked)
    return list

  def CheckAndContinue(self):
    if self.filename!=None and self.demuxer!=None:
      ret=wx.MessageBox('Loaded file not remuxed - Quit anyway ?','Warning',parent=self,style=wx.YES|wx.NO_DEFAULT|wx.CENTRE|wx.ICON_QUESTION)
      if ret!=wx.YES:
        return False
    return True

  def SetFilename(self,dir,file):
    filename=os.path.join(dir,file)
    # TODO: some *quick* oggness checks
    self.filename=filename
    self.SetupActionButton(self.action_button)

  def OnLoadButton(self,event):
    if not self.CheckAndContinue():
      return
    extensions=['ogg','oga','ogv','ogx']
    spec='Ogg streams|'
    for ext in extensions:
      spec+='*.'+ext+';'
      spec+='*.'+ext.capitalize()+';'
      spec+='*.'+ext.upper()+';'
    spec+='|All files|*;*.*'
    loader=wx.FileDialog(self,message='Load Ogg stream',wildcard=spec)
    ret=loader.ShowModal()
    if ret==wx.ID_OK:
      filename=loader.GetFilename()
      dirname=loader.GetDirectory()
      self.SetFilename(dirname,filename)

  def FillKateList(self,demuxer):
    streams=FindKateStreams(demuxer.GetDirectory())
    for idx in streams:
      stream=streams[idx]
      self.list.Append(['','',''])
      list_idx=self.list.GetItemCount()-1
      self.list.SetStringItem(list_idx,0,stream['serial'])
      self.list.SetStringItem(list_idx,1,stream['language'])
      self.list.SetStringItem(list_idx,2,stream['category'])
      self.list.SetItemData(list_idx,int(idx))
    return self.list.GetItemCount()>0

  def OnDemuxButton(self,event):
    try:
      demuxer=Demuxer(self.tools,self.filename,self.options.format)
    except Exception,e:
      wx.MessageBox('Failed to demux file:\n'+str(e),'Error',parent=self,style=wx.OK|wx.CENTRE|wx.ICON_ERROR)
      return

    if not self.FillKateList(demuxer):
      wx.MessageBox('No Kate streams found, or not an Ogg stream','Error',parent=self,style=wx.OK|wx.CENTRE|wx.ICON_ERROR)
      self.filename=None
      self.SetupActionButton(self.action_button)
      return

    self.demuxer=demuxer
    self.SetupActionButton(self.action_button)
    path=os.path.abspath(self.demuxer.GetDirectory())
    wx.MessageBox(
      'Demuxed files are in '+path+'.\n'+
        'Double click on any entry to edit it.\n'+
        'You may also edit them with your favorite text editor.',
      'Edit files',
      parent=self,
      style=wx.OK|wx.CENTRE
    )

  def RemoveTemporaryFiles(self,directory):
    streams=FindKateStreams(directory)
    for idx in streams:
      stream=streams[idx]
      filename=stream['filename']
      os.unlink(filename)
      if os.access(filename+'.ogg',os.F_OK):
        os.unlink(filename+'.ogg')
    os.unlink(os.path.join(directory,'misc.ogg'))
    os.rmdir(directory)

  def OnMuxButton(self,event):
    directory=self.demuxer.GetDirectory()
    try:
      if self.options.save_as_copy:
        root,ext=os.path.splitext(self.filename)
        remuxed_name=root+'.remuxed'+ext
      else:
        remuxed_name=self.filename
      muxer=Muxer(self.tools,remuxed_name,directory,'kate')
      while self.list.GetItemCount()>0: self.list.DeleteItem(0)
      self.demuxer=None
      self.SetupActionButton(self.action_button)
      try:
        self.RemoveTemporaryFiles(directory)
      except:
        wx.MessageBox('Failed to remove all temporary files from\n%s' % directory,'Error',parent=self,style=wx.OK|wx.CENTRE|wx.ICON_ERROR)
    except Exception,e:
      wx.MessageBox('Failed to remux file:\n'+str(e),'Error',parent=self,style=wx.OK|wx.CENTRE|wx.ICON_ERROR)

  def OnOptionsButton(self,event):
    dlg=UIOptions(self,self.options)
#    dlg.CenterOnScreen()
    dlg.ShowModal()
    dlg.Destroy()

  def OnHelpButton(self,event):
    wx.MessageBox(
      kdj_name+' is a remuxing program that allows extracting and decoding Kate tracks from an Ogg stream, '+
      'and recreating that Ogg stream after the Kate streams have been altered.\n'+
      '\n'+
      kdj_name+' requires both the Kate tools (kateenc and katedec) and the oggz tools.'+
      '\n\n'+
      'Click \'Load Ogg stream\' to load an Ogg file from disk. All types of Ogg streams are supported '+
      '(eg, audio, video, etc).\n'+
      '\n'+
      'Then, click \'Demux file\' to extract Kate streams, and decode them to a temporary place on disk. '+
      'You may now edit the decoded Kate streams, either by double clicking on the one you wish to change, '+
      'or with your text editor of choice.\n'+
      '\n'+
      'When done, click \'Remux file from parts\' to reconstruct the Ogg stream with the modified Kate streams. '+
      'If you are notified of any error (eg, syntax errors in the modified Kate streams), then go back and '+
      'correct any mistakes, and click \'Remux file from parts\' until no errors are reported.\n'
      '\n'+
      'You may now play the rebuilt Ogg stream in your audio/video player, and repeat the demux/remux two step '+
      'process until you are happy with the resulting stream.\n'+
      '',
      kdj_name+' help',
      parent=self,
      style=wx.OK|wx.CENTRE
    )

  def OnQuitButton(self,event):
    if not self.CheckAndContinue():
      return
    if self.demuxer!=None and self.options.remove_temporary_files:
      directory=self.demuxer.GetDirectory()
      try:
        self.RemoveTemporaryFiles(directory)
      except:
        wx.MessageBox('Failed to remove all temporary files from\n%s' % directory,'Error',parent=self,style=wx.OK|wx.CENTRE|wx.ICON_ERROR)
    sys.exit(0)

