target = lowm
all: $(target)

%: %.c
	$(CC) -Wall $< -o $@ -lX11

clean:
	$(RM) $(target)
