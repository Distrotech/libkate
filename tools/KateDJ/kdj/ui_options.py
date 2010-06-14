import sys
import wx
from constants import *

base_width=480
option_height=24
help_height=32+16*4
button_height=32
hpadding=16
vpadding=8

class UIOptions(wx.Dialog):
  def __init__(self,parent,options):
    pre=wx.PreDialog()
    pre.Create(parent,wx.ID_ANY,title=kdj_name+' options')
    self.PostCreate(pre)

    self.options=options

    self.y=vpadding
    self.opt_save_as_copy=self.AddCheckBox(
      'Save remuxed file as a copy of the original Ogg file',
      options.save_as_copy,
      'If enabled, remuxed files will be saved on a different file, so the source '+
      'file is left untouched. The new file will be named similarly to the old file, '+
      'but with \'.remuxed\' inserted before the extension.'
    )
    self.y+=vpadding
    self.opt_remove=self.AddCheckBox(
      'Remove all temporary files on exit',
      options.remove_temporary_files,
      'If enabled, any temporary files extracted from an Ogg stream will be, '+
      'removed, even if the file was not remuxed. Any changes that may have '+
      'been made to the Kate streams in these temporary files will be lost.'
    )
    self.y+=vpadding
    self.opt_format=self.AddList(
      'Decode Kate streams as',
      ['Kate', 'SRT', 'LRC'],
      options.format,
      'Selects the text format to use when decoding Kate streams.\n'+
      'Typically, text based movie subtitles are best decoded as SRT, and other '+
      'streams as Kate.'
    )
    self.y+=vpadding

    help_pos=(hpadding,self.y)
    help_size=(base_width,help_height)
    help_inset=(16,24)
    wx.StaticBox(self,label='More info',pos=help_pos,size=help_size)
    help_pos=(help_pos[0]+help_inset[0],help_pos[1]+help_inset[1])
    self.help_size=(help_size[0]-2*help_inset[0],help_size[1]-2*help_inset[1])
    self.help_text=wx.StaticText(self,wx.ID_ANY,'',pos=help_pos,size=help_size)
    self.y+=help_height
    self.y+=vpadding

    self.accept_button=wx.Button(self,wx.ID_OK,'OK',(hpadding,self.y),((base_width-hpadding)/2,button_height))
    self.cancel_button=wx.Button(self,wx.ID_CANCEL,'Cancel',(hpadding*2+base_width/2,self.y),((base_width-hpadding)/2,button_height))
    self.accept_button.Bind(wx.EVT_BUTTON,self.OnOK)
    self.y+=button_height

    self.y+=vpadding
    self.SetSize((base_width+hpadding*2,self.y))
    self.Show(True)

  def SetHelpText(self,text):
    self.help_text.SetLabel(text)

  def SetHelpTextAndSkip(self,text,ev):
    self.help_text.SetLabel(text)
    self.help_text.Wrap(self.help_size[0])
    ev.Skip()

  def AddButton(self,id,label):
    button=wx.Button(self,id=id) #,label=label)
    return button

  def AddCheckBox(self,label,on,help_text):
    checkbox=wx.CheckBox(self,label=label,pos=(hpadding,self.y),size=(base_width,option_height))
    checkbox.Bind(wx.EVT_ENTER_WINDOW,lambda ev: self.SetHelpTextAndSkip(help_text,ev))
    checkbox.Bind(wx.EVT_LEAVE_WINDOW,lambda ev: self.SetHelpTextAndSkip('',ev))
    checkbox.SetValue(on)
    self.y+=option_height
    return checkbox

  def AddList(self,text,choices,default,help_text):
    box=wx.BoxSizer(wx.HORIZONTAL)
    box.SetDimension(hpadding,self.y,base_width,option_height)

    label=wx.StaticText(self,wx.ID_ANY,text)
    box.Add(label,flag=wx.ALIGN_CENTER_VERTICAL)
    label.Bind(wx.EVT_ENTER_WINDOW,lambda ev: self.SetHelpTextAndSkip(help_text,ev))
    label.Bind(wx.EVT_LEAVE_WINDOW,lambda ev: self.SetHelpTextAndSkip('',ev))

    box.AddSpacer((8,0))

    list=wx.Choice(self, -1, (80,-1),choices=choices)
    list.SetStringSelection(default)
    box.Add(list,flag=wx.ALIGN_CENTER_VERTICAL)
    list.Bind(wx.EVT_ENTER_WINDOW,lambda ev: self.SetHelpTextAndSkip(help_text,ev))
    list.Bind(wx.EVT_LEAVE_WINDOW,lambda ev: self.SetHelpTextAndSkip('',ev))

    box.Layout()
    self.y+=option_height
    return list

  def OnOK(self,event):
    self.options.save_as_copy=self.opt_save_as_copy.IsChecked()
    self.options.remove_temporary_files=self.opt_remove.IsChecked()
    self.options.format=self.opt_format.GetStringSelection().lower()
    self.options.Save()
    event.Skip()

