class A
    def dispatch
        return 1
    end
end

class B
    def dispatch
        return 2
    end
end

class C
    def dispatch
        return 3
    end
end

class D
    def dispatch
        return 4
    end
end

a = A.new
b = B.new
c = C.new
d = D.new
1000000.times do
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
end