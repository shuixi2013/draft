
TARGET = count_comment_lines

count_comment_lines: count_comment_lines.c
	gcc -Wall -o $@ $^

clean:
	rm -f *.o
	rm -f $(TARGET)
