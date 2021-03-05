class Counter
    def reset()
        @number = 0
    end

    def up()
        @number += 1
    end
end

counter = Counter.new()
counter.reset()
1000000.times do
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.up()
    counter.reset()
end