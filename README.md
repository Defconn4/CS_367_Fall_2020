# CS_367_Fall_2020

Programming projects from CS 367 - Computer Systems and Programming (C)

## About

The intent of the repo is to showcase some of the more complex projects I had worked on during the Fall 2020 semester. These projects most accurately reflect a real-world application of C.

# Project 1 - Development of custom floating point representation

The intent of this project was to expand upon class discussion of the IEEE standard for floating point representation. Implementation allows conversion of standard C floating point values to a the custom bit-level floating point representation for internal use.

Additional functionality includes the ability to convert custom floating point representation back to standard C float values and basic arithmetic (addition, subtraction, and multiplication).

    Custom Floating Point Bit-Level Representation (Entitled 'MLKY')
    
    MLKY floating points are encoded within a signed 32-bit integer. The lower 15 bits ONLY were used in implementation, thus bits 31-15 are unused and always 0.
    
    1) Bit 14 is used to encode the Sign (S) bit.
    2) Bits 13 - 8 are used to encode the Exponent (EXP).
    3) Bits 7 - 0 are used to encode the Fraction (FRAC).
    
    Ultimately, floating point values are represented as (-1)^S * MANTISSA * 2^EXP. Normalized, Denormalized, and Special values are representable through any operation or input.

# What I Learned

This project focused on the proper implementation of algorithms discussed in class for converting (fractional) binary and decimal values to IEEE floating point representations. There was an heavy (and obvious) focus on bitwise operations and bit shifting to achieve the proper representation of floating point values.

Most importantly, this project was a great dive into an issue you may run into while working in industry, while simulataneously taking me through the steps of designing and implementing a thorough and correct solution. I organized my design by focusing on dismantling the project into digestible chunks, understanding the custom MLKY encoding first. Then, I focused on the conversion of floating points to the our custom MLKY encoding and vice versa, before finally tackling addition, subtraction, and mutiplication in that order.

By far, the designing phase was where I spent the most of my time, but ultimately resulted in much less debugging near the deadline and a final grade of 97/100.

# Project 3 - Custom UNIX shell (MASH - Mason Shell)

## Copyright & Use
I, Frank Costantino, am the sole owner and developer of this code. Use by any other party or individual is strictly prohibited.

Project documentation & code is available upon request.
