# -*- coding: utf-8 -*-
import ujson
import sys
import numpy as np


class Test(object):
    def __init__(self):
        self.a = [1, 2, 3]
        self.b = u'text'
        self.c = (1, 3)
        
        
        
    def toDict(self):
        #print (self.__dict__)
        return self.__dict__
        
        
        

def run():

    c = Test()
    c.toDict()
    
    a = [1, 2, 3]
    ar = np.zeros(5, np.float) + 134.56436789
    ari = np.zeros(5, np.int) + 12
    ari[3] = 5
    ar2 = np.zeros( (3, 2), np.int)
    ar3 = np.ones( (4, 1), np.float)
    npscalar4 = (ar == ari) [0]
    
    s = "other thing. Make this into a fairly long string so I can test that streaming from file actually works. The trick is to make sure that it actually tests breaking this down into pieces and then stitches it together"
    d = {"A": "value", 5: s, 7:a}
    b = ("l", 5)
    d[b] = b
    d['obj'] = c
    
    d['ar'] = ar
    d['ar2'] = ar2
    d['ar3'] = ar3
    d['npscalar4'] = npscalar4
    d['ari'] = ari
    
    e = {}
    e['t'] = a
    e[12] = d
    e[u'yu'] = c
    e['xxx'] = True
    e['opt'] = False
    e['thenone'] = None
    
    e["CMDCompressing"] = u"Сжатие файла %s"
    e["CMDImported"] = u'''Закончен импорт %s в %s как %s. Несжатые рабочие файлы в %s. Рабочие файлы:
%s. Из версии %s'''
    
    keys = e.keys()
    
    print ('== before ==')
    print (sys.getrefcount(e))
    print (sys.getrefcount(a))
    print (sys.getrefcount(d))
    print (sys.getrefcount(d['A']))
    print (sys.getrefcount(b))
    print (sys.getrefcount(keys[0]))
    print (sys.getrefcount(keys[1]))
    print (sys.getrefcount(keys[2]))
    print (sys.getrefcount(c))
    print (sys.getrefcount(c.a))
    print (sys.getrefcount(c.b))
    print (sys.getrefcount(c.c))
    
    #etemp = {}
    #etemp['a'] = ari[1]
    f = open(r'C:\temp\temp_russian.json', 'w')
    for i in range(1):
        print i
        #a = ujson.dumps(e, indent=1, ensure_ascii=1, flush_to_file=1)
        ##print a
        ujson.dump(e, f, indent=1, ensure_ascii=1, flush_to_file=1)
        
    print ( '== after ==')
    print (sys.getrefcount(e))
    print (sys.getrefcount(a))
    print (sys.getrefcount(d))
    print (sys.getrefcount(d['A']))
    print (sys.getrefcount(b))
    print (sys.getrefcount(keys[0]))
    print (sys.getrefcount(keys[1]))
    print (sys.getrefcount(keys[2]))
    print (sys.getrefcount(c))
    print (sys.getrefcount(c.a))
    print (sys.getrefcount(c.b))
    print (sys.getrefcount(c.c))
    
           
def run1():
    
    
    c = Test()
    c.toDict()
    
    a = [1, 2, 3]
    ar = np.zeros(5, np.float) + 134.56436789
    ari = np.zeros(5, np.int) + 12
    ari[3] = 5
    ar2 = np.zeros( (3, 2), np.int)
    ar3 = np.ones( (4, 1), np.float)
    npscalar4 = (ar == ari) [0]
    
    s = "other thing. Make this into a fairly long string so I can test that streaming from file actually works. The trick is to make sure that it actually tests breaking this down into pieces and then stitches it together"
    d = {"A": "value", 5: s, 7:a}
    b = ("l", 5)
    d[b] = b
    d['5'] = c
    
    d['ar'] = ar
    d['ar2'] = ar2
    d['ar3'] = ar3
    d['npscalar4'] = npscalar4
    d['ari'] = ari
    
    e = {}
    #e['t'] = a
    e[12] = d
    #e[u'yu'] = c
    #e['xxx'] = True
    #e['opt'] = False
    #e['thenone'] = None
    
    #e["CMDCompressing"] = u"Сжатие файла %s"
    #e["CMDImported"] = u'''Закончен импорт %s в %s как %s. Несжатые рабочие файлы в %s. Рабочие файлы:
#%s. Из версии %s'''
    
    keys = e.keys()
    
    txt = ujson.dumps(e)
    for i in range(1):
        print i
        
        #retVal = ujson.loads(txt)
        f = open(r'C:\temp\temp_russian.json', 'r')
        retVal = ujson.load(f, stream_from_file=100)
        f.close()
        #print sys.getrefcount(f)
        
    print( retVal)
        
        
def run2():
    
    f = open(r'C:\temp\temp.json', 'r')
    for i in range(1):
        retVal = ujson.load(f)
        
    print( retVal)
    
if __name__ == '__main__':
    run()
    
    run1()
    #run2()
