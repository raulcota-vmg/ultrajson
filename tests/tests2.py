# -*- coding: utf-8 -*-
import ujson
import sys


class Test(object):
    def __init__(self):
        self.a = [1, 2, 3]
        self.b = u'text'
        self.c = (1, 3)
        
        
        
    def toDict(self):
        print (self.__dict__)
        return self.__dict__
        
        
        
    
def run():

    c = Test()
    c.toDict()
    
    a = [1, 2, 3]
    s = "other thing. Make this into a fairly long string so I can test that streaming from file actually works. The trick is to make sure that it actually tests breaking this down into pieces and then stitches it together"
    d = {"A": "value", 5: s, 7:a}
    b = ("l", 5)
    d[b] = b
    d['5'] = c
    
    
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
    
    f = open(r'C:\temp\temp_russian.json', 'w')
    for i in range(1):
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
    
    f = open(r'C:\temp\temp_russian.json', 'r')
    for i in range(1):
        retVal = ujson.load(f, stream_from_file=100)
        
    print( retVal)
        
        
def run2():
    
    f = open(r'C:\temp\temp.json', 'r')
    for i in range(1):
        retVal = ujson.load(f)
        
    print( retVal)
    
if __name__ == '__main__':
    #run()
    
    run1()
    #run2()
