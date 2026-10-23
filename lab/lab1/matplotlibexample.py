# import the neccessary libraries required for the plotting of a graph, such as matplotlib
import matplotlib.pyplot as plt

#list of the x-values and the y-values we want to plot
y = [100,23,234,122]
x = [1,2,3,4]

#the plot() function creates the plot
plt.plot(x,y) #make sure that the x and the y list contains the same number of elements

#xlabel() and ylabel() functions helps in renameing the titles of the x and y axis
plt.xlabel('name the x-axis')
plt.ylabel('name the y-axis')

#include the title of the graph using the title() function 
plt.title('the title of the graph')

#saves the graph as a file 
plt.savefig("nameofthefile.png")

#shows the output of the graph on the screen
plt.show()
