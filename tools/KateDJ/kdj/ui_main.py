#!/usr/bin/env python

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
serial_width=(base_width/4)
language_width=(base_width/4)
category_width=(base_width/2)
button_height=32
list_height=96
hpadding=16
vpadding=16

class UIMain(wx.Frame):
  def __init__(self):
    wx.Frame.__init__(self,None,wx.ID_ANY,kdj_name_version,size=(1,1))

    self.options=Options(wx.StandardPaths.Get().GetUserConfigDir())

    try:
      self.tools=Tools(wx.BeginBusyCursor,wx.EndBusyCursor)
    except Exception,e:
      wx.MessageBox('Failed to find necessary tools:\n'+e.args[0],'Error',style=wx.OK|wx.CENTRE)
      sys.exit(1)

    self.filename=None
    self.demuxer=None
    self.y=vpadding

    self.action_button=self.AddActionButton()
    self.y+=vpadding
    self.AddList()
    self.y+=vpadding

    self.options_button=self.AddOptionsButton()
    self.help_button=self.AddHelpButton()
    self.quit_button=self.AddQuitButton()
    self.y+=button_height

    self.y+=vpadding
    self.SetSize((base_width+hpadding*2,self.y))
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
    button=wx.Button(self,wx.ID_ANY,text,(hpadding,self.y),(base_width,button_height))
    self.y+=button_height
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
    button=wx.Button(self,wx.ID_ANY,'Options',(hpadding,self.y),((base_width-hpadding*2)/3,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnOptionsButton,button)
    return button

  def AddHelpButton(self):
    button=wx.Button(self,wx.ID_ANY,'Help',(hpadding*2+(base_width-hpadding*2)/3,self.y),((base_width-hpadding*2)/3,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnHelpButton,button)
    return button

  def AddQuitButton(self):
    button=wx.Button(self,wx.ID_ANY,'Quit',(hpadding*3+(base_width-hpadding*2)*2/3,self.y),((base_width-hpadding*2)/3,button_height))
    button.Bind(wx.EVT_BUTTON,self.OnQuitButton,button)
    return button

  def AddList(self):
    list=wx.ListCtrl(self,style=wx.LC_REPORT)
    list.SetPosition((hpadding,self.y))
    list.SetSize((base_width,list_height))
    list.ClearAll()
    list.InsertColumn(0,'Serial')
    list.InsertColumn(1,'Language')
    list.InsertColumn(2,'Category')
    list.SetColumnWidth(0,serial_width)
    list.SetColumnWidth(1,language_width)
    list.SetColumnWidth(2,category_width)
    self.list=list
    self.y+=list_height

  def CheckAndContinue(self):
    if self.filename!=None and self.demuxer!=None:
      ret=wx.MessageBox('Loaded file not remuxed - Quit anyway ?','Warning',style=wx.YES|wx.NO_DEFAULT|wx.CENTRE)
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
    spec+='|All files (*.*)|*.*'
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
      idx=self.list.GetItemCount()-1
      self.list.SetStringItem(idx,0,stream['serial'])
      self.list.SetStringItem(idx,1,stream['language'])
      self.list.SetStringItem(idx,2,stream['category'])
    return self.list.GetItemCount()>0

  def OnDemuxButton(self,event):
    try:
      demuxer=Demuxer(self.tools,self.filename,'kate')
    except Exception,e:
      wx.MessageBox('Failed to demux file:\n'+e.args[0],'Error',style=wx.OK|wx.CENTRE)
      return

    if not self.FillKateList(demuxer):
      wx.MessageBox('No Kate streams found, or not an Ogg stream','Error',style=wx.OK|wx.CENTRE)
      self.filename=None
      self.SetupActionButton(self.action_button)
      return

    self.demuxer=demuxer
    self.SetupActionButton(self.action_button)
    path=os.path.abspath(self.demuxer.GetDirectory())
    wx.MessageBox(
      'Demuxed files are in '+path+'.\nYou may edit them with your favorite text editor.',
      'Edit files',
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
        wx.MessageBox('Failed to remove all temporary files from\n%s' % directory,'Error',style=wx.OK|wx.CENTRE)
    except Exception,e:
      wx.MessageBox('Failed to remux file:\n'+e.args[0],'Error',style=wx.OK|wx.CENTRE)

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
      'Then, click \'Demux file\' to extract Kate streams, and decode them to a temporary place on disk '+
      'You may now edit the decoded Kate streams with your text editor of choice.\n'+
      '\n'+
      'When done, click \'Remux file from parts\' to reconstruct the Ogg stream with the modified Kate streams. '+
      'If you are notified of any error (eg, syntax errors in the modified Kate streams), then go back and '+
      'correct any mistakes, and click \'Remux file from parts\' until no errors are reported.\n'
      '\n'+
      'You may now play the rebuilt Ogg stream in your audio/video player, and repeat the demux/remux two step '+
      'process until you are happy with the resulting stream.\n'+
      '',
      kdj_name+' help',
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
        wx.MessageBox('Failed to remove all temporary files from\n%s' % directory,'Error',style=wx.OK|wx.CENTRE)
    sys.exit(0)

