import sys
import os

def FindKateStreams(directory):
    dict={}
    for root, dirs, files in os.walk(directory):
      for file in files:
        if '.kate'==file[-5:]:
          filename=os.path.join(root,file)
          fragments=filename.split('.') # kate.%l.%c.%i.%s.kate
          if fragments and len(fragments)>=6:
            language=fragments[-5]
            category=fragments[-4]
            index=fragments[-3]
            serial=fragments[-2]
            dict[index]={'filename':filename,'language':language,'category':category,'serial':serial}
          else:
            print 'Bad filename format: '+filename

    return dict

