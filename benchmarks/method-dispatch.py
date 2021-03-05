class A:
    def dispatch(self):
        return 1

class B:
    def dispatch(self):
        return 2

class C:
    def dispatch(self):
        return 3

class D:
    def dispatch(self):
        return 4

def main():
    a = A()
    b = B()
    c = C()
    d = D()
    for i in range(1000000):
        a.dispatch()
        b.dispatch()
        c.dispatch()
        d.dispatch()
        a.dispatch()
        b.dispatch()
        c.dispatch()
        d.dispatch()
        a.dispatch()
        b.dispatch()
        c.dispatch()
        d.dispatch()

main()