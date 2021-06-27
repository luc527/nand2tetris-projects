// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel;
// the screen should remain fully black as long as the key is pressed. 
// When no key is pressed, the program clears the screen, i.e. writes
// "white" in every pixel;
// the screen should remain fully clear as long as no key is pressed.

// Put your code here.

// if (RAM[KBD] == 0) color = 0
// else               color = -1

(MAIN)
    @KBD
    D=M
    @ELSE
    D;JEQ
    
    @color
    M=-1

    @PAINT
    0;JMP
    
(ELSE)
    @color
    M=0

(PAINT)
    @SCREEN
    D=A
    @addr
    M=D    // addr = SCREEN

(LOOP)
    @24576   // end address (KBD)
    D=A
    @addr
    D=D-M
    @MAIN
    D;JEQ    // if (addr == 24576) goto MAIN (back to listening the keyboard)

    @color
    D=M
    @addr    // using addr as pointer
    A=M
    M=D      // setting RAM[addr] to color

    @addr
    M=M+1    // next 16-pixel scren segment

    @LOOP
    0;JMP
