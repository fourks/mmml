/*H************************************************************
*  FILENAME:     mmml-desktop-compiler.c
*  DESCRIPTION:  Micro Music Macro Language (μMML) Desktop
*                Compiler
*
*  NOTES:        Converts .mmml (or .txt) files to .mmmldata
*                files for the μMML Desktop Synthesizer
*                program.
*
*                To compile .mmml code, simply run:
*                $ ./compiler -f FILENAME.mmml
*
*  AUTHOR:       Blake 'PROTODOME' Troise
************************************************************H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h> // needed for int types on Linux

#define TOTAL_POSSIBLE_MACROS 255 // (the player can't see more than this)

// total number of channels before macros
#define CHANNELS 4

// terminal print colours
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

signed char   line_counter = -1;

char          *loop_temp_string  = NULL,
              *source_data       = NULL,
              *output            = NULL,
              *tempo_temp_string = NULL,
              *macro_temp_string = NULL;

long          bufsize;

unsigned char header_data[TOTAL_POSSIBLE_MACROS * 2];

unsigned int  data_index[TOTAL_POSSIBLE_MACROS],
              previousDuration = 4; // default duration (if none specified) is a crotchet

unsigned char command,
              channel,
              octave,
              loops,
              macro_line;

unsigned char temp_nibble = 0;

unsigned int  total_bytes,
              line = 1,
              highest_macro,
              loop_value,
              tempo_value,
              macro_value,
              output_data_accumulator;

void error_message(char number, int line)
{
	printf(ANSI_COLOR_RED "[ERROR %d] ",number);

	line++;

	switch(number)
	{
		case 0 :
			printf("Generic error.\n");
			break;
		case 1 :
			printf("File either doesn't exist, or it can't be opened.\n");
			break;
		case 2 :
			printf("No file specified. Use the '-f' flag to chose a file (eg. '-f FILENAME.mmml').\n");
			break;
		case 3 :
			printf("Line %d: Consecutive commands (missing a value).\n\nAlso, there are no two character commands in μMML.\n", line);
			break;
		case 4 :
			printf("Line %d: Consecutive (numerical) values - invalid number.\n\nYou might have entered a number that the command doesn't support. Additionally, the compiler could be expecting this value to be a certain number of digits in length and you might have exceeded that.\n", line);
			break;
		case 5 :
			printf("Line %d: Not a valid note duration.\n\nThe available values here are: 128,64,64.,32,32.,16,16.,8,8.,4,4.,2,2.,1\n", line);
			break;
		case 6 :
			printf("Line %d: Invalid octave value; yours has gone above 'o5'.\n\nThis is easily missed with the '>' command. Check to see where you've incremented over the five octave limit.\n", line);
			break;
		case 7 :
			printf("Line %d: Volumes only range from 1 to 8. (1 quietest - 8 loudest)\n", line);
			break;
		case 8 :
			printf("Line %d: Invalid loop amount, 2 - 255 only.\n\nRemember, a loop refers to how many times the nested material should play, so a value of 0 and 1 would be redundant.\n", line);
			break;
		case 11 :
			printf("Line %d: Invalid octave value; yours has gone below 'o1'.\n\nThis is easily missed with the '<' command. Check to see where you've decremented below the base octave value.\n", line);
			break;
		case 12 :
			printf("Line %d: Invalid macro value.\n\nWhat, is 255 not enough for you?\n", line);
			break;
		case 13 :
			printf("Line %d: Invalid macro value; referred to a macro that doesn't exist.\n", line);
			break;
		case 14 :
			printf("Too few channels stated!\n");
			break;
		case 15 :
			printf("Line %d: Invalid macro value; must be non-zero.\n", line);
			break;
		case 16 :
			printf("Line %d: Invalid tempo; exceeded maximum value.\n\nTempos are represented by 8-bit numbers, therefore the highest value possible is 255.\n", line);
			break;
		case 17 :
			printf("Line %d: Invalid tempo; must be non-zero.\n\nThe compiler will only accept tempi from 1 to 255.\n", line);
			break;
		case 18 :
			printf("Line %d: Invalid loop amount; exceeded maximum value.\n\nLoops are represented by 8-bit numbers, therefore the highest value possible is 255.\n", line);
			break;
		case 19 :
			printf("Line %d: Invalid loop amount; must be non-zero.\n\nThe compiler will only accept tempos from 1 to 255.\n", line);
			break;
		}
		printf(ANSI_COLOR_RESET);

	printf("\nTerminating due to error. Really sorry! μMML Desktop Compiler end.\n\n");

	// Just in case I port this to DOS.
	free(source_data);
	free(output);

	exit(0);
}

void save_output_data(unsigned char input)
{
	output[output_data_accumulator] = input;
	output_data_accumulator++;
}

void write_file(void)
{
	unsigned char data;

	FILE *newfile = fopen("output.mmmldata", "w");

	for (unsigned long i = 0; i < channel * 2; i++)
		fprintf(newfile, "%c", header_data[i]);

	for (unsigned long i = 0; i < output_data_accumulator; i++)
	{
		data = output[i];

		fprintf(newfile, "%c", data);
	}

	fclose(newfile);

	printf(ANSI_COLOR_GREEN "Successfully compiled!" ANSI_COLOR_RESET);	
	printf("\nTotal sequence is %d bytes.", total_bytes);
	printf("\nOutput written to 'output.mmmldata'.\n");
}

void read_file(char *file_name)
{
	FILE *input_file = fopen(file_name, "r");

	if (input_file != NULL)
	{
		printf("Reading file: '%s'.\n", file_name);

		// jump to the end of the input file
		if (fseek(input_file, 0L, SEEK_END) == 0)
		{
			// get size of file
			bufsize = ftell(input_file);

			printf("Input file size: %ld bytes.\n", bufsize);

			if (bufsize == -1)
				error_message(0,0);
	
			// allocate memory
			source_data = (char*)malloc(sizeof(char) * (bufsize + 1));
	
			if (fseek(input_file, 0L, SEEK_SET) != 0)
				error_message(0,0);
	
			// read file into memory
			size_t newLen = fread(source_data, sizeof(char), bufsize, input_file);

			if ( ferror( input_file ) != 0 )
				error_message(1,0);
			else
				source_data[newLen++] = '\0';
		}
		fclose(input_file);
	}
	else
		error_message(1,0);
}

// just keeps things a bit tidier
void next_byte(void)
{
	command = 0;
	total_bytes++;
}

// so rubbish, so poorly implemented
void print_duration(int number)
{
	previousDuration = number;
	switch(number)
	{
		case 1:    //whole note
			temp_nibble = temp_nibble | 0x00;
			save_output_data(temp_nibble);
			break;
		case 2:    //half note
			temp_nibble = temp_nibble | 0x01;
			save_output_data(temp_nibble);
			break;
		case 21:   //dotted half note
			temp_nibble = temp_nibble | 0x08;
			save_output_data(temp_nibble);
			break;
		case 41:   //dotted quarter note
			temp_nibble = temp_nibble | 0x09;
			save_output_data(temp_nibble);
			break;
		case 4:    //quarter note
			temp_nibble = temp_nibble | 0x02;
			save_output_data(temp_nibble);
			break;
		case 81:   //dotted 8th note
			temp_nibble = temp_nibble | 0x0A;
			save_output_data(temp_nibble);
			break;
		case 8:    //8th note
			temp_nibble = temp_nibble | 0x03;
			save_output_data(temp_nibble);
			break;
		case 16:   //16th note
			temp_nibble = temp_nibble | 0x04;
			save_output_data(temp_nibble);
			break;
		case 161:  //dotted 16th note
			temp_nibble = temp_nibble | 0x0B;
			save_output_data(temp_nibble);
			break;
		case 32:   //32nd note
			temp_nibble = temp_nibble | 0x05;
			save_output_data(temp_nibble);
			break;
		case 321:  //dotted 32nd note
			temp_nibble = temp_nibble | 0x0C;
			save_output_data(temp_nibble);
			break;
		case 641:  //dotted 64th note
			temp_nibble = temp_nibble | 0x0D;
			save_output_data(temp_nibble);
			break;
		case 64:   //64th note
			temp_nibble = temp_nibble | 0x06;
			save_output_data(temp_nibble);
			break;			
		case 128:  //128th note
			temp_nibble = temp_nibble | 0x07;
			save_output_data(temp_nibble);
			break;
	}
}

int compiler_core()
{
	/* 
	 *  COMMAND VALUES
	 *  0  :  Awaiting command (a value was the last input).
	 *  1  :  Note / rest was last input.
	 *  2  :  Octave was last input.
	 *  3  :  Volume was last input.
	 *  4  :  Loop was last input.
	 *  5  :  Comment, ignore input.
	 *  6  :  Apparently nothing? I just jumped to 7?
	 *  7  :  Tempo was last input.
	 *  8  :  Macro was last input.
	 *
	 *  The language is essentially a more legible version of the
	 *  nibble structure read by the mmml.c program, so no complex
	 *  interpretation has to be done. The program looks for the
	 *  first nibble, then the second to finish the byte.
	 *
	 *  There are a few commands that behave differently, for
	 *  example, the channel command, the comments, macros, tempo
	 *  loops and the null byte, which signals the end of the program.
	 */

	output = (char*) malloc(bufsize);

	for(unsigned int i = 0; i < bufsize; i++)
	{
		switch(source_data[i])
		{
			/* Report data position */
			case '?' :
				printf("Position: %i, Channel: %i\n",total_bytes-1, channel);
				break;

			/* The return character and counting new lines. Commands are
			 * reset every line (mainly for cancelling the commenting flag). */
			case '\n' :
				line++;
				command = 0;
				break;

			/*  Comment flag. This sets the command value to 5 which, in
			 *  each switch case condition, is checked for to see if the
			 *  character at 'i' should be ignored or not. */
			case '%' :
				command = 5;
				break;

			/* Relative octave command */
			case '>' :
				if(command != 5)
				{
					if(command == 0)
					{
						/* More gotos, I KNOW! I just wanted to add this feature
						 * real quick. It checks to see whether you've put two
						 * octave changes in succession, then ignores the redundant
						 * octave. */
						while(source_data[i+1] == '>')
						{
							octave++;
							i++;
						}

						if(octave >= 5)
							error_message(6,line);
						else if(octave == 4)
							save_output_data(0xDF);
						else if(octave == 3)
							save_output_data(0xD7);
						else if(octave == 2)
							save_output_data(0xD3);
						else if(octave == 1)
							save_output_data(0xD1);

						octave++;
						next_byte();

					}
					else
						error_message(3,line);
				}
				break;

			case '<' :
				if(command != 5)
				{
					if(command == 0)
					{
						while(source_data[i+1] == '<')
						{
							octave--;
							i++;
						}

						if(octave == 5)
							save_output_data(0xD7);
						else if(octave == 4)
							save_output_data(0xD3);
						else if(octave == 3)
							save_output_data(0xD1);
						else if(octave == 2)
							save_output_data(0xD0);
						else if(octave <= 1 || octave > 5)
							error_message(11,line);

						octave--;
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Track flag command */

			case ';' :
				if(command != 5)
				{
					if(command == 0)
					{
						save_output_data(0xFE);	
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Channel command */

			case '@' :
				if(command != 5)
				{
					if(channel > 0)
						save_output_data(0xFF);
					data_index[channel] = total_bytes;
					channel++;

					total_bytes++;
				}
				break;

			/* 
			 *  Note commands. Sharps are found by looking one byte
			 *  ahead. If that byte is a sharp, skip forward one place
			 *  extra and carry on reading, if not, just place the
			 *  natural and continue as normal.
			 */

			case 'r' : // rest command (technically a note)
				if(command != 5)
				{
					if(command == 0)
					{
						temp_nibble = 0x00;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'c' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x20;
						else
							temp_nibble = 0x10;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'd' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x40;
						else
							temp_nibble = 0x30;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'e' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x60;
						else
							temp_nibble = 0x50;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'f' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x70;
						else
							temp_nibble = 0x60;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'g' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x90;
						else
							temp_nibble = 0x80;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'a' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0xB0;
						else
							temp_nibble = 0xA0;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'b' :
				if(command != 5)
				{
					if(command == 0)
					{
						if(source_data[i+1] == '#' || source_data[i+1] == '+')
							temp_nibble = 0x10;
						else
							temp_nibble = 0xC0;
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			/* Octave command */

			case 'o' :
				if(command != 5)
				{
					if(command == 0)
					{
						temp_nibble = 0xD0;
						command = 2;
					}
					else
						error_message(3,line);
				}
				break;

			/* Tempo command */

			case 't' :
				if(command != 5)
				{
					if(command == 0)
					{
						save_output_data(0xF3);
						next_byte();

						tempo_temp_string = (char*)malloc(sizeof(char) * (4));
						
						for(unsigned char p = 0; p < 4; p++)
							tempo_temp_string[p] = '\0';

						for(unsigned char n = 0; n < 4; n++)
						{
							if(isdigit(source_data[i+1]))
							{
								i++;
								tempo_temp_string[n] = source_data[i];
							}
							else{
								if(n == 0)
									error_message(3,line);
								if(n > 3)
									error_message(16,line);
								if(n > 0 && n <= 3)
								{
									tempo_temp_string[n+1] = '\0';

									tempo_value = atoi(tempo_temp_string);

									if(tempo_value > 255)
									{
										error_message(16,line);
									}
									else if(tempo_value == 0)
									{
										error_message(17,line);
									}

									save_output_data(tempo_value);
									free(tempo_temp_string);
								}
								break;
							}
						}
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Volume command */

			case 'v' :
				if(command != 5)
				{
					if(command == 0)
					{
						temp_nibble = 0xE0;
						command = 3;
					}
					else
						error_message(3,line);
				}
				break;

			/* Loop command */

			case '[' :
				if(command != 5)
				{
					if(command == 0)
					{
						save_output_data(0xF0);
						next_byte();

						loop_temp_string = (char*)malloc(sizeof(char) * (4));

						for(unsigned char p = 0; p < 4; p++)
							loop_temp_string[p] = '\0';

						for(unsigned char n = 0; n < 4; n++)
						{
							if(isdigit(source_data[i+1]))
							{
								i++;
								loop_temp_string[n] = source_data[i];
							}
							else{
								if(n == 0)
									error_message(3,line);
								if(n > 3)
									error_message(16,line);
								if(n > 0 && n <= 3)
								{
									loop_temp_string[n+1] = '\0'; // null terminate string

									loop_value = atoi(loop_temp_string);

									if(loop_value > 255)
									{
										error_message(18,line);
									}
									else if(loop_value <= 1)
									{
										error_message(19,line);
									}

									save_output_data(loop_value);
									free(loop_temp_string);
								}
								break;
							}
						}
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			case ']' :
				if(command != 5)
				{
					if(command == 0)
					{
						save_output_data(0xF1);
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Macro command */
			case 'm' :
				if(command != 5)
				{
					if(command == 0)
					{
						save_output_data(0xF2);
						next_byte();

						macro_temp_string = (char*)malloc(sizeof(char) * (4));

						for(unsigned char p = 0; p < 4; p++)
							macro_temp_string[p] = '\0';

						for(unsigned char n = 0; n < 4; n++)
						{
							if(isdigit(source_data[i+1]))
							{
								i++;
								macro_temp_string[n] = source_data[i];
							}
							else{
								if(n == 0)
									error_message(3,line);
								if(n > 3)
									error_message(12,line);
								if(n > 0 && n <= 3)
								{
									macro_temp_string[n+1] = '\0';

									macro_value = atoi(macro_temp_string);
									
									if(macro_value > 255)
										error_message(12,line);
									else if(macro_value == 0)
										error_message(15,line);

									if(macro_value > highest_macro)
									{
										highest_macro = macro_value;
										macro_line = line;
									}

									/* note - macros start at 1 in the compiler and 0
									 * in the AVR code; that's why we minus one below */
									macro_value--;

									save_output_data(macro_value);
									free(macro_temp_string);

								}
								break;
							}
						}
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* 
			 *  Numerical values. Similar to the notes. As the two-character
			 *  decimal value '16' is a valid number the program has to scout
			 *  ahead in the array to work out if the number it's looking at
			 *  is a 1, or a value above 9.
			 */

			case '1' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(source_data[i+1] == '6')
							{
								if(source_data[i+2] == '.')
								{
									print_duration(161);
									i++;
								}
								else
									print_duration(16);
								i++;
							}
							else if(source_data[i+1] == '2' && source_data[i+2] == '8')
							{
								print_duration(128);
								i+=2;
							}
							else
								print_duration(1);
							break;
						case 2: // octave
							temp_nibble = temp_nibble | 0x00;
							save_output_data(temp_nibble);
							octave = 1;
							break;
						case 3: //volume
							temp_nibble = temp_nibble | 0x08;
							save_output_data(temp_nibble);
							break;
					}
					next_byte();
				}
				break;

			case '2' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(source_data[i+1] == '.')
							{
								print_duration(21);
								i++;
							}
							else
								print_duration(2);
							break;
						case 2: // octave
							temp_nibble = temp_nibble | 0x01;
							save_output_data(temp_nibble);
							octave = 2;
							break;
						case 3: //volume
							temp_nibble = temp_nibble | 0x07;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '3' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(source_data[i+1] == '2')
							{
								if(source_data[i+2] == '.')
								{
									print_duration(321);
									i++;
								}
								else
									print_duration(32);
								i++;
							}
							else
								error_message(5,line);
							break;
						case 2: // octave
							temp_nibble = temp_nibble | 0x03;
							save_output_data(temp_nibble);
							octave = 3;
							break;
						case 3: // volume
							temp_nibble = temp_nibble | 0x06;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;
			
			case '4' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							switch(source_data[i+1])
							{
								case '.' :
									print_duration(41);
									i++;
									break;
								default :
									print_duration(4);
									break;
								}
							break;
						case 2: // octave
							temp_nibble = temp_nibble | 0x07;
							save_output_data(temp_nibble);
							octave = 4;
							break;
						case 3: // volume
							temp_nibble = temp_nibble | 0x05;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '5' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2: // octave
							temp_nibble = temp_nibble | 0x0F;
							save_output_data(temp_nibble);
							octave = 5;
							break;
						case 3: // volume
							temp_nibble = temp_nibble | 0x04;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '6' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(source_data[i+1] == '4')
							{
								if(source_data[i+2] == '.')
								{
									print_duration(641);
									i++;
								}
								else
									print_duration(64);
								i++;
							}
							else
								error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							temp_nibble = temp_nibble | 0x03;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '7' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							temp_nibble = temp_nibble | 0x02;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '8' :
				if(command != 5)
				{
					switch(command)
					{
						case 0 :
							error_message(4,line);
							break;
						case 1:
							switch(source_data[i+1])
							{
								case '.' :
									print_duration(81);
									i++;
									break;
								default :
									print_duration(8);
									break;
								}
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							temp_nibble = temp_nibble | 0x01;
							save_output_data(temp_nibble);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '9' :
				if(command != 5)
				{
					switch(command)
					{
						case 0:
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							error_message(7,line);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '0' :
				if(command != 5)
				{
					switch(command)
					{
						case 0:
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(11,line);
							break;
						case 3: //volume
							error_message(7,line);
						break;
					}
				}
				break;
			break;
 
				// ugh, that whole section was terrible
		}

		/*
		 * The condition below checks to see if, firstly,
		 * the last command was a note value, then checks
		 * whether the next symbol is a non-numerical
		 * command. If two commands have been placed
		 * contiguously (ignoring spaces) then the last
		 * specified duration is inserted.
		 *
		 * Basically, it allows the user to omit identical
		 * duration commands in typical MML fashion.
		 *
		 * The stupidly long condition below was because
		 * I got fed up trying to make it more elegant.
		 * I spent a good two hours trying to iterate
		 * through a command array - and I'm too far in
		 * to completely refactor the code. This works,
		 * it's inelegant, but I'm happy to leave it there.
		 */
		if(command == 1)
		{
			if(   source_data[i+1] == 'r' || source_data[i+1] == 'a' || source_data[i+1] == 'b'
			   || source_data[i+1] == 'c' || source_data[i+1] == 'd' || source_data[i+1] == 'e'
			   || source_data[i+1] == 'f' || source_data[i+1] == 'g' || source_data[i+1] == 'o'
			   || source_data[i+1] == '@' || source_data[i+1] == 'm' || source_data[i+1] == 't'
			   || source_data[i+1] == 'v' || source_data[i+1] == '[' || source_data[i+1] == ']'
			   || source_data[i+1] == '%' || source_data[i+1] == '<' || source_data[i+1] == '>'
			   || source_data[i+1] == '\n' )
			{
				print_duration(previousDuration);
				next_byte();
			}
		}
	}

	/* End of file */
	if(channel < (CHANNELS - 1))
		error_message(14, line);

	if(highest_macro > (channel - CHANNELS))
		error_message(13, macro_line);

	/* End final channel/macro */
	save_output_data(0xFF);
	total_bytes++;

	unsigned int v = 0;

	for(unsigned char p = 0; p < channel; p++)
	{
		header_data[v++] = ((data_index[p] + (channel * 2)) & 0xFF00) >> 8;
		header_data[v++] =  (data_index[p] + (channel * 2)) & 0x00FF;
		total_bytes+=2;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	printf("Hello and welcome to the μMML Desktop Compiler! (v1.2)\n\n");

	for (uint8_t i = 1; i < argc; i++){
		if (strcmp(argv[i], "-f") == 0){
			if(i == argc-1)
				error_message(1,0);
			else
				read_file(argv[i+1]);
			i++;
		}
		else
			error_message(2,0);
	}

	printf("\nCompiling...\n");
	compiler_core();

	printf("\nWriting...\n");
	write_file();

	printf("\nμMML Desktop Compiler end.\n\n");

	// Just in case I port this to DOS.
	free(source_data);
	free(output);

	return 0;
}