NAME = ircserv

CXX = c++
# -pedantic: warn about non-standard C++ extensions.
# -MMD: write header dependencies to .d files.
# -MP: add dummy header targets so removed headers do not break make.
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -pedantic -MMD -MP
INCLUDES = -Iinclude

SRCS = main.cpp

SRCDIR = src
OBJDIR = objs
# main.cpp -> objs/main.o
OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)
RM = rm -rf

all: $(NAME)

# Include generated dependency files. '-' ignores missing files on first build.
-include $(OBJS:.o=.d)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# $< is the first prerequisite, $@ is the target file.
# -c compiles without linking; -o $@ writes the object file.
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) $(OBJDIR)

fclean: clean
	$(RM) $(NAME)

# Run all in a recipe so "make -j re" waits for fclean first.
re: fclean
	@$(MAKE) all

.PHONY: all clean fclean re
