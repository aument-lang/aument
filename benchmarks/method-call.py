class Counter:
    def reset(self):
        self.number = 0
    def up(self):
        self.number += 1

def main():
    counter = Counter()
    counter.reset()
    for i in range(1000000):
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

main()