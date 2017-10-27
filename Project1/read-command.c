// UCLA CS 111 Lab 1 command reading

// Copyright 2012-2014 Paul Eggert.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "command.h"
#include "command-internals.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <error.h>

token_stream_t token_stack = NULL;     
command_t *command_stack = NULL;      

/********************************************
** Declaration of useful helping functions **
*********************************************/

char* read_stream(int (*get_next_byte) (void *), void *get_next_byte_argument); 
token_stream_t tokenize(char *buffer); 
void check_tokens(token_stream_t tokenStream);
int check_char(char ch);
command_stream_t make_command_stream_helper(token_stream_t tokenStream);
command_t new_command();
command_t command_combine(command_t command1, command_t command2, token_stream_t tokenStream);
command_stream_t stream_add(command_stream_t commandStream, command_stream_t item);

/**********************************
** Main Function Implementations **
**********************************/

/* make_command_stream() calls several helper functions to create the command stream.
First, it calls a function to read the input stream from the file input and loads it 
into a char buffer. Next, it calls a function to "tokenize" the buffer and produce a 
stream in the form of a linked list. The next makes sure each object in the stream is
valid. Lastly, the function calls another helper function to transform the linked list
into a command stream																	*/
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  char *buffer = read_stream(get_next_byte, get_next_byte_argument);
  struct token_stream *tokenStream;
  struct command_stream *commandStream;
  tokenStream = tokenize(buffer);
  if (tokenStream == NULL)
    { 
      fprintf(stderr, "%s\n", "Error in make_command_stream(): returning an empty token stream");
      return NULL;
    }
  check_tokens(tokenStream);
  commandStream = make_command_stream_helper(tokenStream);
  return commandStream;
}

/* read_command_stream() looks at the current command stream and returns a pointer to 
the current node in the linked list. Then it uses recursion to return the next pointer 
in the stream and so on.																*/
command_t
read_command_stream (command_stream_t s)
{
  if (s == NULL)
    return NULL;
  else if (s->is_read == 0) 
    {
      s->is_read = 1;	
      return (s->command);
    }
  else if (s->next != NULL)
    return read_command_stream(s->next);	
  else
    return NULL;
}

/**************************
** Stack Implementations **
***************************/

/* token_push() takes the argument item and attempts to append it onto the token stream
linked list.																			*/
void token_push(token_stream_t item)
{
	if (item == NULL)
		return;
	if (token_stack == NULL)
	{
		token_stack = item;
		token_stack->prev = token_stack->next = NULL;
	}
	else
	{
		item->next = NULL;
		item->prev = token_stack;

		token_stack->next = item;
		token_stack = item;
	}
}

/* token_top() looks at the last item in the token stream linked list and returns its type */
enum token_type token_top()	
{
	if (token_stack == NULL)
		return UNKNOWN_TOKEN;
	else
		return token_stack->token.type;
}

/* token_pop() attempts to remove the last item in the token stream linked list	*/
token_stream_t token_pop()	
{
	if (token_stack == NULL)
		return NULL;
	token_stream_t top = token_stack;
	token_stack = token_stack->prev;
	if (token_stack != NULL)
		token_stack->next = NULL;
	top->prev = top->next = NULL;
	return top;
}

/* command_push() sets the input item to the last item of the command "stack" array */
void command_push(command_t item, int *top, size_t *size)	
	if (item == NULL)
		return;
	if (*size <= (*top + 1) * sizeof(command_t))
		command_stack = (command_t *)checked_grow_alloc(command_stack, size);
	(*top)++;
	command_stack[*top] = item;
}

/* command_pop() removes the last item of the command "stack" array */
command_t command_pop(int *top)
{
	if (*top == -1)
		return NULL;
	command_t item = NULL;
	item = command_stack[*top];
	(*top)--;
	return item;
}

/* stack_precedence() looks at the input token type and uses a switch statement to return
the appropriate precedence of the token in the stack by using a numbering system		*/
int stack_precedence(enum token_type type)
{
	switch (type)	
	{
	case LEFT_PAREN_TOKEN:
		return 1;
	case UNTIL_TOKEN:
	case WHILE_TOKEN:
	case IF_TOKEN:
		return 2;
	case THEN_TOKEN:
	case DO_TOKEN:
		return 3;
	case ELSE_TOKEN:
		return 4;
	case LESS_THAN_TOKEN:
	case GREATER_THAN_TOKEN:
		return 8;
	case PIPE_TOKEN:
		return 9;
	case NEWLINE_TOKEN:
	case SEMICOLON_TOKEN:
		return 11;
	default:
		return 0;
	}
}

/* stream_precedence() looks at the input token type and uses a switch statement to return
the appropriate precedence of the token in the stream by using a numbering system		*/
int stream_precedence(enum token_type type)
{
	switch (type)
	{
	case LEFT_PAREN_TOKEN:
		return 14;
	case UNTIL_TOKEN:
	case WHILE_TOKEN:
	case IF_TOKEN:
		return 13;
	case THEN_TOKEN:
	case DO_TOKEN:
		return 12;
	case ELSE_TOKEN:
		return 10;
	case LESS_THAN_TOKEN:
	case GREATER_THAN_TOKEN:
		return 7;
	case PIPE_TOKEN:
		return 6;
	case NEWLINE_TOKEN:
	case SEMICOLON_TOKEN:
		return 5;
	default:
		return 0;
	}
}

/************************************
** Helper Function Implementations **
************************************/

/* print_error() just prints the error message and the line where it occurs */
void print_error(int lineNumber)
{
	fprintf(stderr, "%i: Error\n", lineNumber);
}

/* read_stream() loops through the input, places each character of the input into a 
char buffer, and then returns that buffer											*/
char* read_stream(int (*get_next_byte) (void *), 
                       void *get_next_byte_argument)
{
  char ch;
  size_t size = 1024;
  size_t index = 0;  
  char *buffer = (char *) checked_malloc(sizeof(char) * size); 
  while ((ch = get_next_byte(get_next_byte_argument)) != EOF) 
    {
      buffer[index] = ch;
      index++;
      if (index == size)
	buffer = (char *) checked_grow_alloc(buffer, &size);
    }
  if (index == size - 1)
    buffer = (char *) checked_grow_alloc(buffer, &size);
  buffer[index] = '\0';
  return buffer;
}

/* tokenize() goes through the char buffer and identifies each "token" or operator 
and inserts them into a linked list that is the token stream						*/
token_stream_t tokenize(char *buffer)
{
  int index = 0;
  int lineNumber = 1;
  enum token_type type;
  struct token_stream *head = NULL;
  struct token_stream *curr = head;
  if (buffer[index] == '\0') 
    return NULL;
  while (buffer[index] == '\n')
    {
      lineNumber++;
      index++;
    }
  char charIndex = buffer[index];
  while (buffer[index] != '\0')
    {
      charIndex = buffer[index];
      switch (charIndex)
	{
	case '(':
	  type = LEFT_PAREN_TOKEN;
	  break;
	case ')':
	  type = RIGHT_PAREN_TOKEN;
	  break;
	case '<':
	  type = LESS_THAN_TOKEN;
	  break;
	case '>':
	  type = GREATER_THAN_TOKEN;
	  break;
	case ';':
	  type = SEMICOLON_TOKEN;
	  break;
	case '|':
	  type = PIPE_TOKEN;
	  break;
	case ' ': 
	case '\t':
	  index++;
	  continue;  
	case '\n':
	  type = NEWLINE_TOKEN;
	  lineNumber++;         
	  if (buffer[index + 1] == '\n')
	    {
	      index++; 
	      continue; 
	    }
	  break;
	case '#':
		if (index > 0 && check_char(buffer[index - 1]))
		{
			print_error(lineNumber);
			exit(1);
		}
		else
		{
			int cnt = 1;
			while ((buffer[index + cnt] != '\n') && (buffer[index + cnt] != '\0'))
				cnt++;
			index = index + cnt + 1;
			continue;
		}
	default:
	  type = UNKNOWN_TOKEN; 
	} 
      int lngth = 1;
      int temp = index;
      if (check_char(charIndex))	
	{
	  type = WORD_TOKEN;
	  while (check_char(buffer[index + lngth]))
	    lngth++;
	  index += lngth - 1;
	}
      struct token_stream *stream = (struct token_stream *) checked_malloc(sizeof(struct token_stream)); 
      stream->prev = NULL;
      stream->next = NULL;
      stream->token.type = type;
	  stream->token.length = lngth;
      if (type == NEWLINE_TOKEN)
	stream->token.line_num = lineNumber - 1;	
      else
	stream->token.line_num = lineNumber;
      if (type == WORD_TOKEN)
	{
	  stream->token.word = (char *) checked_malloc(sizeof(char) * lngth + 1);
	  int i = 0;
	  while (i < lngth)
	    {
	      stream->token.word[i] = buffer[temp + i];
	      i++;
	    }
	  stream->token.word[i] = '\0';
	  if (strstr(stream->token.word, "if") != NULL && lngth == 2)
	    stream->token.type = IF_TOKEN;
	  else if (strstr(stream->token.word, "then") != NULL && lngth == 4) 
	    stream->token.type = THEN_TOKEN;
	  else if (strstr(stream->token.word, "else") != NULL && lngth == 4) 
	    stream->token.type = ELSE_TOKEN;
	  else if (strstr(stream->token.word, "fi") != NULL && lngth == 2) 
	    stream->token.type = FI_TOKEN;
	  else if (strstr(stream->token.word, "while") != NULL && lngth == 5) 
	    stream->token.type = WHILE_TOKEN;
	  else if (strstr(stream->token.word, "do") != NULL && lngth == 2) 
	    stream->token.type = DO_TOKEN;
	  else if (strstr(stream->token.word, "done") != NULL && lngth == 4) 
	    stream->token.type = DONE_TOKEN;
	  else if (strstr(stream->token.word, "until") != NULL && lngth == 5) 
	    stream->token.type = UNTIL_TOKEN;
	}
      else if (type == UNKNOWN_TOKEN)	
	{
	  print_error(lineNumber);
	  exit(1);
	}
      if (curr == NULL)
	{
	  head = stream;
	  curr = head;
	}
      else
	{
	  stream->prev = curr;
	  stream->next = NULL;
	  curr->next = stream;
	  curr = stream;
	}
      index++;
    }
  return head; 
}

/* check_tokens() checks the syntax of the token stream to check for any errors in the
ordering or placement of tokens and prints out error statements if there are any		*/
void check_tokens(token_stream_t tokenStream)
{
  enum token_type nextToken;
  enum token_type prevToken;
  int numParentheses = 0; 
  int numIf = 0;
  int numDone = 0;
  token_stream_t curr = tokenStream;
  token_stream_t nextStream = NULL;
  token_stream_t prevStream = NULL;
  while(curr != NULL)
    {
      nextStream = curr->next;
      if (nextStream != NULL)
	nextToken = nextStream->token.type;
      prevStream = curr->prev;
      if (prevStream != NULL)
	prevToken = prevStream->token.type;
      switch (curr->token.type)
	{
	case WORD_TOKEN:
		if (nextToken == IF_TOKEN || nextToken == THEN_TOKEN || nextToken == ELSE_TOKEN || nextToken == FI_TOKEN || nextToken == WHILE_TOKEN || nextToken == UNTIL_TOKEN || nextToken == DO_TOKEN || nextToken == DONE_TOKEN)
		{
			nextToken = WORD_TOKEN;
		}
		break;
	case SEMICOLON_TOKEN:
		if (curr == tokenStream || nextToken == SEMICOLON_TOKEN)
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		break;
	case PIPE_TOKEN:
		if (curr == tokenStream || nextToken == SEMICOLON_TOKEN || curr->token.type == nextToken)
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		break;
	case LEFT_PAREN_TOKEN:
          if (nextToken == RIGHT_PAREN_TOKEN)
	    {
	      print_error(curr->token.line_num);
	      exit(1);
	    }
          numParentheses++;	
          break;
	case RIGHT_PAREN_TOKEN:
          numParentheses--;
          break;
	case GREATER_THAN_TOKEN:
	case LESS_THAN_TOKEN:  
          if (prevStream  == NULL || nextToken != WORD_TOKEN || nextStream == NULL) 
		  {
			print_error(curr->token.line_num);
			exit(1);
          }
          break;
	case DONE_TOKEN:
		if (nextToken == WORD_TOKEN)
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		numDone--;
		break;
	case FI_TOKEN:
		if (nextToken == WORD_TOKEN)
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		numIf--;
		break;
	case IF_TOKEN:
		if (nextToken == SEMICOLON_TOKEN || nextStream == NULL) 
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		numIf++;
		break;
	case WHILE_TOKEN:
	case UNTIL_TOKEN:
		if (nextToken == SEMICOLON_TOKEN || nextStream == NULL) 
		{
			print_error(curr->token.line_num);
			exit(1);
		}
		numDone++;
		break;
	case THEN_TOKEN:
	case ELSE_TOKEN:
          if (nextToken == FI_TOKEN || nextStream == NULL) 
		  {
			print_error(curr->token.line_num);
			exit(1);
          }
	case DO_TOKEN:
		if ((prevToken != SEMICOLON_TOKEN && prevToken != NEWLINE_TOKEN) || curr->token.type == nextToken || nextToken == SEMICOLON_TOKEN || nextStream == NULL)
		  {
			print_error(curr->token.line_num);
            exit(1);
          }         
          if (prevToken == WORD_TOKEN)
	    {
	      curr->token.type = WORD_TOKEN;
	    }
          break;
	case NEWLINE_TOKEN:
		if (nextStream == NULL)
			break;
		if (nextToken == LEFT_PAREN_TOKEN || nextToken == RIGHT_PAREN_TOKEN || nextToken == WORD_TOKEN)
		{
			if ((numParentheses != 0 && prevToken != LEFT_PAREN_TOKEN) || (numIf != 0 && !(prevToken == IF_TOKEN || prevToken == THEN_TOKEN || prevToken == ELSE_TOKEN)) || (numDone != 0 && !(prevToken == WHILE_TOKEN || prevToken == UNTIL_TOKEN || prevToken == DO_TOKEN)))
				curr->token.type = SEMICOLON_TOKEN;
		}
		else
		{
			switch (nextToken)	
			{
			case LEFT_PAREN_TOKEN:
			case RIGHT_PAREN_TOKEN:
			case IF_TOKEN:
			case THEN_TOKEN:
			case ELSE_TOKEN:
			case FI_TOKEN:
			case WHILE_TOKEN:
			case DO_TOKEN:
			case DONE_TOKEN:
			case UNTIL_TOKEN:
				break;
			default:
				print_error(curr->token.line_num);
				exit(1);
			}
		}
		break;
	default:
          break;
	} 
      curr = nextStream;
    } 
  if (numParentheses != 0 || numIf != 0 || numDone != 0) 
    {
	  print_error(prevStream->token.line_num);
      exit(1);
    }
} 

/* make_command_stream_helper() takes the token stream and uses two stacks: a token stack 
and a command stack, to sort the order of the commands. Once we reached the end of a command,
signaled by newlines or left parenthesis or semicolons, we would pop one token and two 
commands and combine them with a helper function. The result would be pushed back onto the 
command stack.																				*/
command_stream_t make_command_stream_helper(token_stream_t tokenStream)
{
  token_stream_t curr = tokenStream;
  token_stream_t nextStream = NULL;
  token_stream_t prevStream = NULL;
  command_stream_t tempStream = NULL;
  command_stream_t commandStream = NULL;
  enum token_type nextToken;
  command_t command1, command2, command3, command4, command5, command6;
  command1 = command2 = command4 = command5 = command6 = NULL;
  int numParentheses = 0;
  int numIf = 0;
  int numWhile = 0;
  int numUntil = 0;
  char **word = NULL;
  int top = -1;
  size_t command_stackSize = 10 * sizeof(command_t);
  command_stack = (command_t *) checked_malloc(command_stackSize);
  while (curr != NULL)
    {
      prevStream = curr->prev;
      nextStream = curr->next;
      if (nextStream != NULL)
	    nextToken = nextStream->token.type;
      switch (curr->token.type) 
	{
	case WORD_TOKEN:
	  if (command1 == NULL)
	    {
	      command1 = new_command();
	      word = (char **) checked_malloc(200 * sizeof(char *)); 
	      command1->u.word = word;
	    }
	  *word = curr->token.word;
	  *(++word) = NULL;           
	  break;
	case SEMICOLON_TOKEN: 
	  command_push(command1, &top, &command_stackSize);    
	  while (stack_precedence(token_top()) > 
		 stream_precedence(curr->token.type))
	    {
	      command5 = command_pop(&top);
	      command4 = command_pop(&top);
	      command1 = command_combine(command4, command5, token_pop());
	      command_push(command1, &top, &command_stackSize);
	    }
	  command1 = NULL;
	  word = NULL;
	  if ( ! (nextToken == THEN_TOKEN || nextToken == ELSE_TOKEN || nextToken == FI_TOKEN || nextToken == DO_TOKEN || nextToken == DONE_TOKEN))
	    {
	      token_push(curr);
	    }
	  break;
	case PIPE_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  while (stack_precedence(token_top()) > stream_precedence(curr->token.type))
	    {
	      command5 = command_pop(&top);
	      command4 = command_pop(&top);
	      command1 = command_combine(command4, command5, token_pop());
	      command_push(command1, &top, &command_stackSize);
	    }
	  command1 = command5 = command4 = NULL;
	  word = NULL;
	  token_push(curr);
	  break;
	case LEFT_PAREN_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  command1 = NULL;
	  word = NULL;
	  numParentheses++;
	  token_push(curr);
	  break;
	case RIGHT_PAREN_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  numParentheses--;
	  while (token_top() != LEFT_PAREN_TOKEN)
	    {
	      command5 = command_pop(&top);
	      command4 = command_pop(&top);
	      command1 = command_combine(command4, command5, token_pop());  
	      command_push(command1, &top, &command_stackSize);
	      command1 = NULL;
	    }
	  command2 = new_command();
	  command2->type = SUBSHELL_COMMAND;
	  command2->u.command[0] = command_pop(&top); 
	  command_push(command2, &top, &command_stackSize);
	  command1 = command2 = NULL;
	  word = NULL;
	  token_pop();
	  break;
	case LESS_THAN_TOKEN:
	case GREATER_THAN_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  if (nextStream != NULL && nextStream->token.type == WORD_TOKEN)
	    {
	      command1 = command_pop(&top);
	      if (curr->token.type == LESS_THAN_TOKEN)
		    command1->input = nextStream->token.word;
	      else if (curr->token.type == GREATER_THAN_TOKEN)
		    command1->output = nextStream->token.word;
	      command_push(command1, &top, &command_stackSize);
	      command1 = NULL;
	      word = NULL;
	      curr = nextStream;
	      if (curr != NULL)
		nextStream = curr->next;
	    }
	  break;
	case WHILE_TOKEN:
	case UNTIL_TOKEN:
	case IF_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  command1 = NULL;
	  word = NULL;
	  if (curr->token.type == WHILE_TOKEN)
	    numWhile++;
	  else if (curr->token.type == UNTIL_TOKEN)
	    numUntil++;
	  else
	    numIf++;
	  token_push(curr);
	  break;
	case THEN_TOKEN:
	case ELSE_TOKEN:
	case DO_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  command1 = NULL;
	  word = NULL;
	  token_push(curr);
	  break;
	case DONE_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  if (token_top() == DO_TOKEN)
	    {
	      command1 = command_pop(&top);
	      token_pop();
	    }
	  if (token_top() == WHILE_TOKEN || token_top() == UNTIL_TOKEN)
	    {
	      command2 = command_pop(&top);
	    }
	  command3 = new_command();
	  if (token_top() == WHILE_TOKEN)
	    {
	      command3->type = WHILE_COMMAND;
	      numWhile--;
	    }
	  else
	    {
	      command3->type = UNTIL_COMMAND;
	      numUntil--;
	    }
	  command3->u.command[0] = command2; 
	  command3->u.command[1] = command1;
	  command_push(command3, &top, &command_stackSize);
	  command1 = command2 = command3 = NULL;
	  word = NULL;
	  token_pop();
	  break;
	case FI_TOKEN:
	  command_push(command1, &top, &command_stackSize);      
	  if (token_top() == ELSE_TOKEN)
	    {
	      command6 = command_pop(&top);
	      token_pop();
	    }   
	  if (token_top() == THEN_TOKEN)
	    {
	      command5 = command_pop(&top);
	      token_pop();
	    }
	  if (token_top() == IF_TOKEN)
	    {
	      command4 = command_pop(&top);
	    }
	  command1 = new_command();
	  command1->type = IF_COMMAND;                
	  if (command6 != NULL)                
	    command1->u.command[2] = command6;
	  else
	    command1->u.command[2] = NULL;
	  command1->u.command[1] = command5; 
	  command1->u.command[0] = command4;   
	  command_push(command1, &top, &command_stackSize);
	  numIf--; 
	  command1 = command4 = command5 = command6 = NULL;
	  word = NULL;
	  token_pop();  
	  break;
	case NEWLINE_TOKEN:
	  command_push(command1, &top, &command_stackSize);
	  while (stack_precedence(token_top()) > 
		 stream_precedence(curr->token.type))
	    {
	      command5 = command_pop(&top);
	      command4 = command_pop(&top);
	      command1 = command_combine(command4, command5, token_pop());
	      command_push(command1, &top, &command_stackSize);
	    }
	  if (numParentheses == 0 && numIf == 0 && numWhile == 0
	      && numUntil == 0)
	    {
	      tempStream = (command_stream_t) checked_malloc(sizeof(struct command_stream));
	      tempStream->command = command_pop(&top);
	      commandStream = stream_add(commandStream, tempStream);
	    }
	  command1 = NULL;
	  word = NULL;
	  break;
	default:
	  break;
	}
      curr = nextStream; 
    } 
  command_push(command1, &top, &command_stackSize);
  while (token_top() != UNKNOWN_TOKEN) 
    {
      command5 = command_pop(&top);
      command4 = command_pop(&top);
      command1 = command_combine(command4, command5, token_pop());
      command_push(command1, &top, &command_stackSize);
    }
  tempStream = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  tempStream->command = command_pop(&top);
  commandStream = stream_add(commandStream, tempStream);
  command1 = NULL;
  return commandStream;
}

/* check_char() checks whether the input char is valid syntax */
int check_char(char ch)
{
  if isalnum(ch)
	return 1;
  switch (ch)
    {
    case '!':   
	case '%':   
	case '+':   
	case ',':   
	case '-':   
	case '_':
    case '.':   
	case '/':   
	case ':':   
	case '@':   
	case '^':
      return 1;
    }
  return 0;
}

/* new_command() creates a new, empty simple command item */
command_t new_command()
{
  command_t item = (command_t) checked_malloc(sizeof(struct command));
  item->type = SIMPLE_COMMAND;
  item->status = -1;
  item->input = item->output = NULL;
  item->u.word = NULL;
  return item;  
}

/* command_combine() creates a new command and places the two into commands into it, thus 
combining the input commands																*/
command_t command_combine(command_t command1, command_t command2, token_stream_t tokenStream)
{
  command_t combined = NULL;
  combined = new_command();
  combined->u.command[0] = command1;
  combined->u.command[1] = command2;
  switch (tokenStream->token.type)
    {
    case SEMICOLON_TOKEN: combined->type = SEQUENCE_COMMAND; break;
    case PIPE_TOKEN: combined->type = PIPE_COMMAND; break;
    case RIGHT_PAREN_TOKEN: combined->type = SUBSHELL_COMMAND; break;
    default: 
      print_error(tokenStream->token.type); 
      exit(1);
    }
  return combined;
}

/* stream_add() adds the input stream onto the input command stream, thus combining them.
If there was nothing in the command stream, the input stream becomes the command stream. */
command_stream_t stream_add(command_stream_t commandStream, command_stream_t item)
{
  if (item == NULL)
    return commandStream;
  if (commandStream == NULL)
    {
      commandStream = item;
      commandStream->prev = item;
      commandStream->next = NULL;
      commandStream->is_read = 0;
    }
  else
    {
      item->prev = commandStream->prev;
      item->next = NULL;
      item->is_read = 0;
      (commandStream->prev)->next = item;
      commandStream->prev = item;
    }
  return commandStream;
}