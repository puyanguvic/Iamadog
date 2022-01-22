import numpy as np

file = open("exp3-dsr2-1.txt","r")
Time = []
Delay = []

for line in file:
    time, string, delay = (item for item in line.split(' '))
    Time.append(time)
    Delay.append(delay)


file.close()

    