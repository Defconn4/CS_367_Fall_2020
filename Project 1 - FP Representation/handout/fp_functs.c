/* Fill in your Name and GNumber in the following two comment fields
 * Name: Frankie Costantino
 * GNumber: G01132886
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>	// Cannot use math.h functions. Cannot use UNIONS either.
#include "fp.h"

// Function prototypes for helper functions.
float powers(int base, int exponent);
void decimal_to_binary(int arr[], int decimal, int is_frac);
int get_bit(int val, int index);
void set_bits_fpgmu(int bit_begin, int bit_end, int arr[], int arr_size, fp_gmu *fpmgu);
void handle_overflow(fp_gmu *fpgmu, float val, int mult_flag);
int binary_to_decimal(fp_gmu val, int bit_begin, int bit_end, int num_bits);
void handle_underflow(fp_gmu *fpgmu, float val, int mult_flag);
void arithmetic_rules_special(fp_gmu *fpgmu, fp_gmu source1, fp_gmu source2, int exp1, int exp2, float frac1, float frac2, int mult_flag);
void encode_nan(fp_gmu *fpgmu);
void arithmetic_rules_zero(fp_gmu *fpgmu, fp_gmu source1, fp_gmu source2, int exp1, int exp2, float frac1, float frac2, int mult_flag);

/* input: float value to be represented
 * output: fp_gmu representation of the input float
 * NOTE: This function performs ASSIGNMENT OPERATIONS (e.g. x = 3.4)
 * Follow the Project Documentation for Instructions
 *
 * NOTES: val is MANTISSA to begin!
 * Handling the value as NORMALIZED to begin:
 * M = 1.frac
 * frac = M - 1
 * E = exp - bias
 * exp = E + bias
 * bias = 2^(e-1) -1
 */
fp_gmu compute_fp(float val) {
	
	// Initialize fp_gmu value, mantissa (M), E and bias. 
	// Overall format is: -1^(S) * M * 2^(E)
	fp_gmu fpgmu = 0;	// Since the initial value is 0, bits [31,15] are already set to 0, so you only need to modify bits [14,0].
	float M = 0.0;		// NORMALIZED: mantissa = [1.0, 2.0), M = 1.frac // DENORMALIZED: mantissa = [0.0, 1.0), M = 0.frac
	int E = 0;		// NORMALIZED: E = exp - bias // DENORMALIZED: E = 1 - bias
	int bias = 0; 		// Always computed as: (2^(e-1)) - 1 // e is the number of bits in the exp field (e = 6 = EXPONENT_BITS)
	
	// Initilaize sign (S), frac, and exp.
	int S = 0;
	float frac = 0.0;
	int exp = 0;
	
	// Check if val is negative so we can change the sign bit (14th) of fpgmu to 1.
	// And change value to positive for handling.
	if(val < 0) { fpgmu |= (1 << 14); val *= -1; }
	
	// Copy val before it is modified.	
	float val_copy = val;

	// Compute the bias (this step is done regardless if NORMALIZED or DENORMALIZED).
	bias = powers(2, (EXPONENT_BITS-1)) - 1;	// Bias is 31 for fp_gmu.

	// If val is positive we want to do this right shifting, if it's negative we want to do left shifting.	
	// We need to right shift val (M) until it is in the range of [1.0, 2.0).
	// Remember, right shifting is the same as dividing by 2 to x power (e.g. 3 >> 2 = 3/(2^2) ).
	while(!(val >= 1.0 && val < 2.0))		// while val is NOT in the range of [1.0, 2.0), we continuously divide by 2.
	{
	
		// if the value is 0 (or -0), we know that our value needs to be handled as underflow.
		if(val == 0)
		{
			// Break and let val be caught below.
			break;
		}
		
		// if the value is less than 1 to begin with, we need to left shift it to get into the range of [1.0, 2.0).
		if(val < 1)
		{
			// Left shift the value, which is the same as multiplying by 2.
			val *= 2.0;
			
			// Everytime we multiply by 2, we need to decrement E.
			E--;
		}
		
		// Handles values > 1.
		else
		{
			val /= 2.0;
			
			// Everytime we divide by 2, we need to increment E to preserve the original value.
			E++;
		}
	}
		
	// Val is now in the range of M = [1.0, 2.0), copy val into M.
	M = val;
	
	// We currently have: S, M, E, bias. We need: frac and exp.
	exp = E + bias;		// Compute exp.
	
	
	// DENORMALIZED: if exp <= 0, handle as a denormalized value.
	if(exp <= 0 || val == 0)
	{
		// Make a call to our helper function that handles underflow (DENORMALIZED) values.
		handle_underflow(&fpgmu, val_copy, 0);
			
		// Return the encoded DENORMALIZED fpgmu value.			
		return fpgmu;
		
	}
	
	// SPECIAL: if exp > 62 (exp is 111 111 = 63), handle as SPECIAL value (exp is too large for normalized).
	else if(exp > 62)
	{
		// Call helper function to handle overflows.
		handle_overflow(&fpgmu, val_copy, 0);
		
		// Cannot do any further computation on SPECIAL values, return fpgmu.
		return fpgmu;
	
	}	// Continue handling value as NORMALIZED.
	
	// Frac is equivalent to M - 1.
	frac = M - 1;

	
	// Now that we have our frac, we need to adjust it to fit into our 8 bit frac field.
	// We need to adjust 8 bits by muliplying by 2^8.
	frac *= powers(2, 8);		// REMEMBER: Our denominator is now 2^8 = 256.
	
	
	// Store frac in an integer to truncate it ==> THIS IS THE ROUND TO ZERO.
	int trunc_frac = frac;
	
	
	// Store trunc_frac into frac field of fpgmu (change 8 bits - [7,0]).
	// First, initialize int array of size 8 (8 bits in frac field).
	int frac_arr[8] = {0,0,0,0,0,0,0,0};
		
	// Convert trunc_frac into binary to be stored in fpgmu.
	decimal_to_binary(frac_arr, trunc_frac, 1);

	// Use our helper function to set the frac field (bits [7,0]).
	set_bits_fpgmu(7, 0, frac_arr, 8, &fpgmu);
		
	// Do the same thing for the exp field now as done above.
	int exp_arr[6] = {0,0,0,0,0,0};
		
	// Convert exp into binary to be stored in fpgmu.
	decimal_to_binary(exp_arr, exp, 0);
		
	// Use our helper function to set the exp field (bits [13,8]).
	set_bits_fpgmu(13, 8, exp_arr, 6, &fpgmu);
			
	return fpgmu;
}
	
	
/* Helper function for handling underflows (DENORMALZIED VALUES) of fpgmu in the compute_fp() function.
 */
void handle_underflow(fp_gmu *fpgmu, float val, int mult_flag) {
	
	// If round to zero is smaller than the smallest representable denormalized value, then it will
	// round to zero and become either 0 (sign bit is 0) or -0 (sign bit is 1).
	// Initialize information for handling:
	int bias = 31;
		
	float M = 0;
	int E = 0;
		
	float frac = 0.0;
	
	// Exp for the frac field is always 0. Since fpgmu was initialized to 0, nothing needs to change here.
	int exp = 0;
	
	// Change the sign bit (14th bit) if val is negative.
	if(val < 0 && mult_flag == 0) { *fpgmu |= (1 << 14); }
	
	// Calculate DENORMALIZED E (E = -30 always):
	E = 1 - bias;	
	int count = 0;
	
	// Need to left shift our value left E times in order to get it into the proper range for DENORMALIZED M = [0.0,1.0).
	// Left shift is the same as multiplying by 2 (# of times left shifted is the power 2 is raised to).
	while(count != E)
	{
		// Do not need to decrement E since this is denormalized.
		val *= 2;
		
		// Decrement count until it is the same value as E.	
		count--;		
	}

	// Currently the value is -1^(S) * val * 2^E
	// Now that val is in the correct range for M in DENORMALIZED ([0.0, 1.0)).
	// Get the correct value for frac by left shifting 8 bits so it can fill the entire frac field of fp_gmu.
	frac = val;
	frac *= powers(2,8);
	
	// Truncate frac properly by initializing our trunc_frac int which automatically round to 0.
	int trunc_frac = frac;
	
	// Initialize our frac_array that helps us set the 8 bits for the frac field (bits [7,0]).
	int frac_arr[8] = {0,0,0,0,0,0,0,0};
	
	// After we round to zero if our value is too small to be represented by DENORMALZIED values, then we need to
	// ensure that the frac bits are all set to zero (which since we have initialized our frac_arr to all 0s to start
	// we can just call our helper function set_bits_fpgmu to set our frac bits to 0.	
	if(trunc_frac == 0)
	{
		set_bits_fpgmu(7, 0, frac_arr, 8, fpgmu);
	}
	
	// Call helper function decimal_to_binary() function to convert our trun_frac to binary representation in 8 bits.
	//int frac_arr[8] = {0,0,0,0,0,0,0,0};
	decimal_to_binary(frac_arr, trunc_frac, 1);
		
	// Use our helper function to set the frac field (bits [7,0]).
	set_bits_fpgmu(7, 0, frac_arr, 8, fpgmu);
	
	
	// Make sure exp is 0, since its denormalized.
	// exp field should already be zero since fpgmu was initialized to zero to begin with.	
	return;
}

/* Helper function for handling overflows (SPECIALIZED VALUES) of fpgmu in the compute_fp() function.
 * Encodes our fp_gmu value as INFINITY or -INFINITY.
 */
void handle_overflow(fp_gmu *fpgmu, float val, int mult_flag) {
	
	
	// Set the exp field (bits [13,8]) of fpgmu to all 1s, regardless of sign bit.
	int index = 0;
	for(index = 13; index >= 8; index--)
	{
		*fpgmu |= (1 << index);	
	}
	
	// Reset index to 0.
	index = 0;
	
	// Set the frac field (bits [7,0]) of fpgmu to all 0s.
	for(index = 7; index >= 0; index --)
	{
		*fpgmu &= ~(1 << index); 
	}
	
	
	// Set the sign bit of fpgmu.
	// If the value is positive, fpgmu was initialized to 0, so the sign bit is still 0, so I don't need to change it.
	// If the value is negative, I need to change the sign bit (14th bit) to 1.\
	// If the operation is multiplication, we already set the sign bit in the mult_vals() function and we do not want to change it here.
	// All other functions that call this helper function BESIDES ANYTHING RELATING TO MULT_VALS() will pass in a 0.
	if(val < 0 && mult_flag == 0)
	{
		*fpgmu |= (1 << 14);	
	}
	
	return;
}



/* Helper function that sets the bits of our frac accordingly.
 * It takes [bit_begin, bit_end] and a pointer to our frac_array / exp_array and fpgmu.
 */
void set_bits_fpgmu(int bit_begin, int bit_end, int arr[], int arr_size, fp_gmu *fpgmu) {

	// Initialize loop variables.
	int i = 0;
	int j = 0;
		
	for(i = bit_begin; i >= bit_end; i--)
	{
		// Sets the i-th but of fpgmu to the value at the current (j-th) index of arr[] (frac_arr or exp_arr).
		while(j < arr_size)
		{
			*fpgmu |= (arr[j] << i);
			
			// Increment j and break so we can move to the next bit of the exp field in fpgmu.
			j++;
			break;	
		}		
	}
}	


/* Helper function to return the bit at index from integer value.
 * Return value should be either 0 or 1.
 */
int get_bit(int val, int index) {

	return (val >> index) & 1;
}


/* Helper function for converting decimal values to binary.
 * Stores the resulting binary into an int array.
 */
void decimal_to_binary(int arr[], int decimal, int is_frac) {
	
	// Initialize loop variable and array size..
	int i = 0;
	int array_size = 0;
	
	// Get the size of the array.
	// If is_frac is 1, we are dealing with the frac field, so the array_size will be 8.
	// If is_frac is 0, we are dealing with the exp field, so the array_size will be 6.
	if(is_frac == 1) { array_size = 8; }
	else { array_size = 6; }
	
	// Convert decimal value to binary, stores it 1 for 1 into arr.
	for(i = array_size-1; decimal > 0; i--)
	{
		arr[i] = decimal % 2;
		decimal = decimal / 2;
	}

}



/* Helper function for computing exponential powers.
 * NOTE: base is raised to the power of exponent: base^exponent.
 */
float powers(int base, int exponent) {

	// If the exponent is 0, return 1.
	if(exponent == 0) { return 1; }
	
	// Initilaize loop counter & result.
	int i = 1;
	float result = base;
		
	// If exponent is negative:
	if(exponent < 0)
	{
		// Make exponent positive and make result into a fraction
		// for mulitplication (so if result = 1/2, we multiply 1 by (1/2) exponent times).
		exponent *= -1;
		result = (1/result);
		
		float neg_result = 1.0;
		
		for(i = 1; i <= exponent; i++)
		{
			neg_result *= result;
		}
		
		return neg_result;
	}
	
	// Compute base to power of exponent.
	for(i = 1; i < exponent; i++)
	{
		result *= base;
	}
	
	// Return modified base.
	return result;
}


/* Helper function to convert binary to decimal.
 * Format is [bit_begin, bit_end].
 */
int binary_to_decimal(fp_gmu val, int bit_begin, int bit_end, int num_bits) {

	// Initialize loop variables.
	int i = 0;
	int j = num_bits-1;
	int result = 0;
	int bit = 0;
	
	for(i = bit_begin; i >= bit_end; i--)
	{
		// Track index for powers of 2.
		while(j >= 0)
		{
			// Extract the bit from the given index.  
			bit = get_bit(val, i);
			
			// Multiply the bit (0 or 1) by 2 to the index of the current bit.
			result += (bit * powers(2,j));
				
			// Decrement j.
			j--;
			
			// Break so we can move to the next bit in the field.
			break;
		}
	}
	
	return result;
}

/* input: fp_gmu representation of our floats
 * output: float value represented by our fp_gmu encoding
 *
 * If your input fp_gmu is Infinity, return the constant INFINITY
 * If your input fp_gmu is -Infinity, return the constant -INFINITY
 * If your input fp_gmu is NaN, return the constant NAN
 *
 * Follow the Project Documentation for Instructions
 */
float get_fp(fp_gmu val) {
	
	// If the value is negative, change the sign bit (14th bit) to 1.
	if(val < 0)
	{
		val |= (1 << 14);
	}

	// Extract the sign bit (14th bit) from val.
	int S = get_bit(val, 14);
	int neg = 1;
	if(S == 1) { neg = -1; }
		
	// Bias from previous calculations and initalize E.
	int bias = 31;
	int E = 0;
	
	float M = 0;
		
	// Extract the frac from the fp_gmu value using our helper function to convert binary to decimal.
	// We are converting the frac field to decimal (which are bits [7,0]).
	float frac = binary_to_decimal(val, 7, 0, 8);
		
	// This is the truncated frac (round to zero).
	int trunc_frac = frac;	
	
	// Divide frac by the number of bits it is contained within to get the proper decimal value.
	// This is the same value as my trunc_frac value in compute_fp().
	frac /= powers(2,8);
	
	// Our Mantissa (M) is going to be 1 + frac.
	//float M = 1 + frac;
	// Calculate exp.	
	int exp = binary_to_decimal(val, 13, 8, 6);
	
	// Check for special values (where exp is all 1s = 111 111) ==> exp = 63.
	if(exp > 62)
	{
		// If frac is 0 and the value is positive, the value is INF.
		if((trunc_frac == 0) && (neg == 1))
		{
			return INFINITY;
		}
		
		// If the frac is 0 and the value is negative, the value is NEGATIVE INF.
		if((trunc_frac == 0) && (neg == -1))
		{
			return -INFINITY;	
		}
		
		// If the frac is not zero, regardless of sign, the value is Not A Number (NaN).
		if(trunc_frac != 0)
		{
			return NAN;	
		}
	}
		
	// DENORMALIZED: If exp is 0, then the value is DENORMALIZED calculate E = 1 - bias.
	else if(exp <= 0)
	{
		E =  1 - bias;
		
		// Denormalized M is equal to the frac.
		M = frac;
	}
	
	// NORMALIZED: Otherwise, the value is normalized and handle E = exp - bias.
	else
	{
		E = exp - bias;
		
		// Normalized M is equal to 1 + frac.
		M = 1 + frac;
	}
	
	
	// Compute our final value before returning.
	float result = neg * M * powers(2,E);
	return result;
}

/* input: Two fp_gmu representations of our floats
 * output: The multiplication result in our fp_gmu representation
 *
 * You must implement this using the algorithm described in class:
 *   Xor the signs: S = S1 ^ S2
 *   Add the exponents: E = E1 + E2
 *   Multiply the Frac Values: M = M1 * M2
 *   If M is not in a valid range for normalized, adjust M and E.
 *
 * Follow the Project Documentation for Instructions
 */
fp_gmu mult_vals(fp_gmu source1, fp_gmu source2) {
	
	// Initialize new fp_gmu value to store product of multiplication.
	// To begin: S = 0, exp = 000 000 (6 bits), frac = 0000 0000 (8 bits).
	fp_gmu fpgmu = 0;
	
	// Always start out by assuming these values are NORMALIZED.
	// For multiplication:
	// S = S1 ^ S2 (xor)
	// M = M1 * M2 (adjust if range is outside of that for NORMALIZED => either /2.0 or *2.0.
	// E = E1 + E2
	float M1 = 0;
	int S1 = get_bit(source1, 14);
	float frac1 = 0.0;
	int exp1 = 0;
	int E1 = 0;
	
	float M2 = 0;
	int S2 = get_bit(source2, 14);
	float frac2 = 0.0;
	int exp2 = 0;
	int E2 = 0;
	
	int bias = 31;		// Bias is always 31, previously computed.
	
	// Float product will hold the product of M1 * M2.
	float product = 0.0;
	float product_frac = 0.0;
	int trunc_frac = 0;
	
	// Set a flag that tells our helper functions if we are coming from the multiplication function.
	int mult_flag = 0;
	
	// Set the sign bit of fpgmu.
	// If XOR of the signs results in 1, change the sign bit of fpgmu to 1 (meaning the value is negative).
	if( (S1 ^ S2) == 1) { fpgmu |= (1 << 14); }
	
	
	// NORMALIZED RULES:
	// M = 1.frac
	// E = exp - bias
	// exp = E + bias
	
	// Compute frac, M, and exp for source1.
	frac1 = binary_to_decimal(source1, 7, 0, 8);
	frac1 /= powers(2,8);
	M1 = 1 + frac1;
	exp1 = binary_to_decimal(source1, 13, 8, 6);

	// Compute frac, M, and exp for source2.
	frac2 = binary_to_decimal(source2, 7, 0, 8);
	frac2 /= powers(2,8);
	M2 = 1 + frac2;
	exp2 = binary_to_decimal(source2, 13, 8, 6);
	
	// Check for arithmetic cases for individual operands being INF/-INF or NAN.
	if(exp1 > 62 || exp2 > 62)
	{
	
		// Set mult_flag to 1.
		mult_flag = 1;
		
		// Call our helper function to handle SPECIAL arithmetic values.
		arithmetic_rules_special(&fpgmu, source1, source2, exp1, exp2, frac1, frac2, mult_flag);	
	
		return fpgmu;
	
	}
	
	// Check for arithmetic cases for individual operands being 0 or -0.
	if((exp1 <= 0 && frac1 == 0.0) || (exp2 <= 0 && frac2 == 0.0))
	{
		// Set mult_flag to 1.
		mult_flag = 1;
		
		// Call our helper function to handle arguments that are ZERO.
		arithmetic_rules_zero(&fpgmu, source1, source2, exp1, exp2, frac1, frac2, mult_flag);	
		
		return fpgmu;
	
	}
	
	// If our values are fine, we can continue to compute as NORMALIZED.
	// Compute E for source1 and source2.
	E1 = exp1 - bias;
	E2 = exp2 - bias;
	
	// Get the sum of Es of source1 and source2.
	int product_E = E1 + E2;
	
	// Get the product of M1 * M2.
	product = M1 * M2;
	
	// Align product if it's outside the range of NORMALIZED M [1.0, 2.0).
	while(!(product >= 1.0 && product < 2.0))
	{
		// If product is less than 1, we need to multiply it by 2 to get it into range.
		if(product < 1)
		{
			product *= 2.0;
			
			// Decrement our product_E when we multiply.
			product_E--;	
		}
		
		// If product is greater than 2, we need to divide by 2 to get it into range.
		else
		{
			product /= 2.0;
			
			// Increment our product_E when we divide.
			product_E++;
		}
	}
	
	// Compute the final exp for the product.
	int product_exp = product_E + bias;
	
	
	// Check the product_exp for UNDERFLOW or OVERFLOW:
	if(product_exp <= 0)
	{
		// Call our helper function handle_underflow to handle denormalzied values.
		// We need to pass in 1 in this function ONLY.
		handle_underflow(&fpgmu, product, 1);
		
		return fpgmu; 
	
	
	}
	
	// Handling special values.
	else if(product_exp > 62)
	{
		// Handle special values.
		handle_overflow(&fpgmu, product, 1);
		
		return fpgmu;
	
	}
	
	
	// Get the frac for the resulting product.
	product_frac = product - 1;
		
	// Multiply product_frac by 2^8 to shift it into 8 bits.
	product_frac *= powers(2,8);
		
	// ROUND TO ZERO by storing product_frac in our int trunc_frac.
	trunc_frac = product_frac;
	
	// Store trunc_frac into frac field of fpgmu (change 8 bits - [7,0]).
	// First initialize int array of size 8.
	int frac_arr[8] = {0,0,0,0,0,0,0,0};
	
	// Convert trunc_frac into binary to be stored in fpgmu.
	decimal_to_binary(frac_arr, trunc_frac, 1);
	
	// Use our helper function to set the frac fiel (bits [7,0]).
	set_bits_fpgmu(7, 0, frac_arr, 8, &fpgmu);
	
	// Do the same as we did above for exp field.
	int exp_arr[6] = {0,0,0,0,0,0};
	
	// Convert exp into binary to be stored in fpgmu.
	decimal_to_binary(exp_arr, product_exp, 0);
	
	// Use our helper function to set exp field (bits [13. 8]).
	set_bits_fpgmu(13, 8, exp_arr, 6, &fpgmu);	
	
	// Return our encoded value for product.
  	return fpgmu;
}

/* input: Two fp_gmu representations of our floats
 * output: The addition result in our fp_gmu representation
 *
 * You must implement this using the algorithm described in class:
 * If needed, adjust the numbers to get the same exponent E
 * Add the two adjusted Mantissas: M = M1 + M2
 * Adjust M and E so that it is in the correct range for Normalized
 * 
 * Mantissa (M) = M1 + M2
 * Sign (S) = Result of Addition
 * Exponent (E): 
 *
 * Follow the Project Documentation for Instructions
 */
fp_gmu add_vals(fp_gmu source1, fp_gmu source2) {
	
	// Initialize new fp_gmu value to store sum of our addition.
	fp_gmu fpgmu = 0;
		
	// We always start by assuming that these values are NORMALIZED.
	// M = 1.frac
	// E = exp - Bias
	// exp = E + Bias
	// Initialize S, M, E, for source 1 and source 2.
	float M1 = 0;
	int S1 = get_bit(source1, 14);
	float frac1 = 0.0;
	int exp1 = 0;
	int E1 = 0;
		
	float M2 = 0;
	int S2 = get_bit(source2, 14);
	float frac2 = 0.0;
	int exp2 = 0;
	int E2 = 0;
	
	int bias = 31;		// Bias calculated in compute_fp(), is always 31.
	
	// This will hold the larger E value.
	int conform_E = 0;
	
	// Float to hold the sum of M1 and M2. neg_flag denotes if our resulting sum is negative, which I
	// will in aid when rounding at the end of the function.
	float sum = 0.0;
	int neg_flag = 1;
	
	// sum_frac will hold the frac our new fp_gmu encoding. trunc_frac will be for our round to zero truncation.
	float sum_frac = 0.0;
	int trunc_frac = 0;
	
	// If exp > 62 then we have a special value, call handleOverflow().
	// Grab the frac of source1.
	frac1 = binary_to_decimal(source1, 7, 0, 8);
	
	// Compute the M1 for source1.
	// Divide frac1 by 2^8 to get the proper M1 computation.
	frac1 /= powers(2,8);
	M1 = 1 + frac1;
	
	// Check the sign bit for M2, if it's 1, we need to multiply M1 by -1.
	if(S1 == 1)
	{
		M1 *= -1;
	}
	
	// Grab the exp of source1.
	exp1 = binary_to_decimal(source1, 13, 8, 6);
	
	// Grab the frac of source2.
	frac2 = binary_to_decimal(source2, 7, 0, 8);
	
	// Compute the M2 for source2.
	// Divide frac2 by 2^8 to get the proper M2 computation.
	frac2 /= powers(2,8);
	M2 = 1 + frac2;
	
	// Check the sign bit for M2, if it's 1, we need to multiply M2 by -1.
	if(S2 == 1)
	{
		M2 *= -1;
	}
	
	// Grab the exp of source2.
	exp2 = binary_to_decimal(source2, 13, 8, 6);
	
	// Check to see if the indiviudal operands are SPECIAL.
	if(exp1 > 62 || exp2 > 62)
	{	
		// Call our helper function to handle SPECIAL values.
		arithmetic_rules_special(&fpgmu, source1, source2, exp1, exp2, frac1, frac2, 0);
		return fpgmu;
	}
	
	// Check arithmetic rules for operands as ZEROES.
	// If either operand is 0 or -0, we want to use our helper function to encode fpgmu.
	// For now, we don't care about the sign bit, just if the exp is in the range for DENORMALIZED
	// since 0 is a DENORMALIZED value and that frac is 0.
	if((exp1 <= 0 && frac1 == 0.0) || (exp2 <= 0 && frac2 == 0.0))
	{
		// Call our helper function to handle ZERO as arguments.
		arithmetic_rules_zero(&fpgmu, source1, source2, exp1, exp2, frac1, frac2, 0);
		
		return fpgmu;
	}					
		
	
	
	// Calculate the NORMALIZED E for both source1 and source2.
	E1 = exp1 - bias;
	E2 = exp2 - bias;
	
	// For NORMALZIED values, just add them as is:
	// Find the larger E so we can shift M correctly.
	// NOTE: Made this >= from > if it breaks on me.
	if(E1 >= E2)
	{
		// If E1 is the larger E, set our E to conform to to E1.
		conform_E = E1;
		
		// If E1 is larger, that means we need to change M2's E to conform_E.
		// Right shift is division, which means increment E.
		// Left shift is multiplication, which means decrement E.
		while(E2 != conform_E)
		{
			// Divide M2 by 2 continually until E2 is the same as E1.
			M2 /= 2.0;
			E2++;
		}
		
	}
	else
	{	
		// Else, set E2 to the E to conform to.
		conform_E = E2;
		
		// If E2 is larger, that means we need to change M1's E to conform_E.
		while(E1 != conform_E)
		{
			// Divide M1 by 2 continually until E1 is the same as E2.
			M1 /= 2.0;
			E1++;
		}
		
	}

	// Get the sum of M1 and M2.
	sum = M1 + M2;
	
	// If our sum is negative, we want to make it positive for aiding in rounding.
	// Also set neg_flag to -1 so we know we need to remultiply our sum by -1 before returning.
	if(sum < 0)
	{
		sum *= -1;
		neg_flag = -1;
	
		// Set the sign bit of fpgmu to 1 (negative).
		fpgmu |= (1 << 14);
	}
	
	// If our final M is not in the correct range for NORMALZIED M [1.0, 2.0), then we need to adjust it.
	while(!(sum >= 1.0 && sum < 2.0))
	{
		
		// If our value is < 1 (is value equal zero would have been caught by now), we need to multiply our sum
		// and decrement conform_E until it is in the proper range.
		if(sum < 1)
		{
			sum *= 2.0;
			conform_E--;
		}
		
		else
		{
			// Divide our sum by 2.
			sum /= 2.0;
		
			// Increment our conform_E, which is what we will be using for encoding.
			conform_E++;
		}
	}
	
	
	// Get our frac for encoding our fp_gmu value.
	// For NORMALIZED values: frac = M - 1
	sum_frac = sum - 1;
	
	// We need to fit our frac encoding into 8 bits, so we need multiply sum_frac by 2^8 (same as left shiting 8 times).
	sum_frac *= powers(2,8);			// REMEMBER: Our denominator is now 2^8 = 256.
	
	// This is our TRUNCATION for our frac encoding (trunc_frac is an int).
	trunc_frac = sum_frac;

	// Compute the final exp for our NORMALIZED encoding.
	// exp = E + bias;
	int final_exp = conform_E + bias;
	
	// Here, we also need to check if the final_exp is DENORMALIZED.
	if(final_exp <= 0)
	{
			
		// Call our helper function handle_underflow to handle denormalized values.	
		handle_underflow(&fpgmu, sum, 0);
		
		// Return our encoded fpgmu value.
		return fpgmu;
	}
	
	// See if the final_exp is greater than 62, meaning that we are handling SPECIAL values.
	// We will only return INF or -INF here, we do not encode for NAN here.
	else if(final_exp > 62)
	{	
		// Check if neg_flag is -1, meaning that at some point we mulitplied the sum by
		// -1 to make the rounding easier.
		// If neg_flag is -1, we need to re-multiply our sum by -1 before we pass it to our helper function.
		if(neg_flag == -1)
		{
			// Return sum to its negative form.
			sum *= -1;
		}
		
		handle_overflow(&fpgmu, sum, 0);
			
		// Return our fpgmu encoded value.
		return fpgmu;
		
	}
	
	// Otherwise, we computing our value as NORMALIZED.	
		
	// For an encoding, we need trunc_frac (bits [7,0]), exp (bits [13,8]), and S (neg_flag).
	// Store trunc_frac into frac field of fpgmu (change 8 bits - [7,0]).
	// First initilaize int array of size 8 (8 bits in frac field).
	int frac_arr[8] = {0,0,0,0,0,0,0,0};
	
	// Convert trunc_frac into frac field of fpgmu.
	decimal_to_binary(frac_arr, trunc_frac, 1);
	
	// Use our helper function to set the frac bits of fpgmu (bits [7,0]).
	set_bits_fpgmu(7, 0, frac_arr, 8, &fpgmu);
		
	// Do the same thing for the exp field now as done above.
	int exp_arr[6] = {0,0,0,0,0,0};
	
	// Convert exp into binary to be stored in fpgmu.
	decimal_to_binary(exp_arr, final_exp, 0);
	
	// Use our helper function to set the exp field (bits [13,8]).
	set_bits_fpgmu(13, 8, exp_arr, 6, &fpgmu);
	
	return fpgmu;
}
	

/**
 * Helper function for handling arithmetic cases where the arguments of add_vals() are some variation of 0.
 *
 * Rules for Zero addition:
 * -0 + -0 = 0
 * -0 + 0 = 0
 *  0 + -0 = 0 
 *  
 *  If you add any value that is non NAN, non INF (-INF too) to 0 or -0, you return that other value.
 *  0 + norm = norm
 *  norm + 0 = norm
 *
 *  -0 + norm = norm
 *  norm + -0 = norm
 */
void arithmetic_rules_zero(fp_gmu *fpgmu, fp_gmu source1, fp_gmu source2, int exp1, int exp2, float frac1, float frac2, int mult_flag) {
	
	// REMEMBER: fpgmu is initialized to 0, so we only need to change the sign.
	// Get the sign bit of source1 and source2.
	int S1 = get_bit(source1, 14);
	int S2 = get_bit(source2, 14);
	
	// xor sign bits for multiplication.
	int xor_sign = S1 ^ S2;
	
	// Check if we are checking operands for multiplication function.
	// REMEMBER, FPGMU COMES IN AS 0, ALL YOU NEED TO DO IS CHANGE THE SIGN TO NEGATIVE IF NEEDED.
	if(mult_flag == 1)
	{
		// The following IF statements are for ZERO MULTIPLICATION.
		
		// If we have 0 * -0, return 0 or -0:
		if(exp1 <= 0 && frac1 == 0.0 && S1 == 0 && exp2 <= 0 && frac2 == 0 && S2 == 1)
		{
			
			// If the xor of the signs is 1, change fpgmu's sign bit (14th bit) to 1.
			if(xor_sign == 1) { *fpgmu |= (1 << 14); }
			
			// Since fpgmu is already 0 when initialized, all we have to do is change the sign bit and return.
			return;
			
		}
		
		// If we have -0 * 0, return 0 or -0:
		if(exp1 <= 0 && frac1 == 0.0 && S1 == 1 && exp2 <= 0 && frac2 == 0 && S2 == 0)
		{
			// If the xor of the signs is 1, change fpgmu's sign bit to 1.
			if(xor_sign == 1) { *fpgmu |= (1 << 14); }
			
			return;
		}	
	
		// The following IF statements are for multiplying any NON-INF or NON-NAN number by 0 or -0.
		
		// If we have 0/-0 * norm #:
		// source 1 is 0 or 0.
		if(exp1 <= 0 && frac1 == 0 && (S1 == 0 || S1 == 1) && exp2 <= 62)
		{
			// If the xor of the signs is 1, change fpgmu's sign bit to 1.	
			if(xor_sign == 1) { *fpgmu |= (1 << 14); }
			
			return;
		}
		
		
		// If we have norm # * 0/-0:
		// source2 is 0 or -0.	
		if(exp2 <= 0 && frac2 == 0 && (S2 == 0 || S2 == 1) && exp1 <= 62)
		{
			// If the xor of the signs is 1, change fpgmu's sign bit to 1.	
			if(xor_sign == 1) { *fpgmu |= (1 << 14); }
		
			return;
		}	
	
	}
	
	// If we are adding -0 and -0, we return 0.
	// -0 means the following:
	// exp <= 0
	// frac = 0
	// S = 1, S = 0 (for 0).
	if(exp1 <= 0 && frac1 == 0 && S1 == 1 && exp2 <= 0 && frac2 == 0 && S2 == 1)
	{
		// Since fpgmu is already initialized to 0, we will just return to the caller and return fpgmu which
		// has not been changed yet by the time this function is called.
		return;
	}
	
	// If we are adding -0 and 0, return 0.
	// We will take -0 as the first argument.
	if(exp1 <= 0 && frac1 == 0 && S1 == 1 && exp2 <= 0 && frac2 == 0 && S2 == 0)
	{
		// Set fpgmu equal to source 2 (0).
		*fpgmu = source2;
		
		return;	
	}
	
	// If we are adding 0 and -0, return 0.
	// We will take -0 as the second argument.
	if(exp2 <= 0 && frac2 == 0 && S2 == 1 && exp1 <= 0 && frac1 == 0 && S1 == 0)
	{
		// Set fpgmu equal to source1 (0).
		*fpgmu = source1;
		
		return;
	}
	
	// If we are adding 0 and any normal value, we return the normal value (non INF, non NAN).
	// Normal value in argument 2.
	if(exp1 <= 0 && frac1 == 0 & S1 == 0 && exp2 > 0 && exp2 <= 62)
	{
		// Set fpgmu to source2.
		*fpgmu = source2;
		
		return;	
	}
	
	// If we are adding any normal value and 0, return normal value.
	// Normal value in argument 1.
	if(exp2 <= 0 && frac2 == 0 && S2 == 0 && exp1 > 0 && exp1 <= 62)
	{
		// Set fpgmu to source1.
		*fpgmu = source1;
		
		return;	
	}
		
	// If we are adding -0 and a normal value, return normal value.
	// Normal value is argument 2.
	if(exp1 <= 0 && frac1 == 0 & S1 == 1 && exp2 > 0 && exp2 <= 62)
        {
		// Set fpgmu to source2.
		*fpgmu = source2;
			
		return;
	}
	
	// If we are adding any normal value and -0, return normal value.
	// Normal value in argument 1.
	if(exp2 <= 0 && frac2 == 0 && S2 == 1 && exp1 > 0 && exp1 <= 62)
	{
		
		// Set fpgmu to source1.
		*fpgmu = source1;
		
		return;		
	} 
	
	return;
}



/**
 * Helper function to encode an fp_gmu value to NaN.
 * For fp_gmu types NaN means:
 *
 * Disregard sign bit.
 * exp ==> Bits [13,8] ==> 6 bits are all set to 1.
 * frac ==> Bits [7,0] ==> 8 bits are set to NOT zero (will be 0000 0001).
 *
 */
void encode_nan(fp_gmu *fpgmu) {
	
	// Set the exp bits of fpgmu all to 1s.
	int i = 0;
	for(i = 13; i >= 8; i--)
	{
		*fpgmu |= (1 << i);
	}
	
	// Set the 0th bit (LSB exp) to 1.
	*fpgmu |= (1 << 0);
	
	// Set the exp bits [7,1] equal to 0.
	for(i = 7; i >= 1; i--)
	{
		*fpgmu &= ~(1 << i);
	}	

	return;
}

/**
 * Helper function for handling the addition arithmetic rules for SPECIAL values.
 *
 * Rules for SPECIAL values:
 *
 * INF + -INF (either order), return NaN.
 * If either arugment is NaN, return NaN.
 * If you add any value (non NaN, -INF) to INF, return INF.
 * If you add any value (non NAN, INF) to -INF, return INF.
 *
 * NOTES:
 *
 * exp > 62 if we are here so:
 * --> if frac == 0 and S == 0, we have INF
 * --> if frac == 0 and S == 1, we have -INF
 * --> if frac != 0, we have NaN
 */
void arithmetic_rules_special(fp_gmu *fpgmu, fp_gmu source1, fp_gmu source2, int exp1, int exp2, float frac1, float frac2, int mult_flag) {
	
	// Get the sign bit of source1 and source2.
	int S1 = get_bit(source1, 14);
	int S2 = get_bit(source2, 14);
		
	// xor sign bits for multiplication.
	int xor_sign = S1 ^ S2;
	
	// Check if we are checking operands for multiplication function.
	if(mult_flag == 1)
	{
		// If either argument is NAN, return NAN.
		if((exp1 > 62 && frac1 != 0) || (exp2 > 62 && frac2 != 0))
		{
			// Call our helper function to encode fpgmu as NAN.
			encode_nan(fpgmu);
		
			return;
		}
		
		// The below IF statements are for INF * 0 or -INF * 0.
		// If we have INF * 0, return NAN.
		// source1 is INF.
		if(exp1 >= 62 && frac1 == 0.0 && S1 == 0 && exp2 <= 0 && frac2 == 0 && S2 == 0)
		{
			// Encode fpgmu as NAN.
			encode_nan(fpgmu);
			
			return;
		}
		
		
		// If we have -INF * 0, return NAN.
		// source1 is -INF.
		if(exp1 >= 62 && frac1 == 0.0 && S1 == 1 && exp2 <= 0 && frac2 == 0 && S2 == 0)
		{
			// Encode fpgmu as NAN.
			encode_nan(fpgmu);
				
			return;
		}
			
		// If we have 0 * INF, return NAN.
		// source2 is INF.
		if(exp2 >= 62 && frac2 == 0.0 && S2 == 0 && exp1 <= 0 && frac1 == 0 && S1 == 0)
		{
			// Encode fpgmu as NAN.
			encode_nan(fpgmu);
			
			return;
		}
		
		// If have 0 * -INF, return NAN.
		// source2 is -INF.	
		if(exp2 >= 62 && frac2 == 0.0 && S2 == 1 && exp1 <= 0 && frac1 == 0 && S1 == 0)
		{
			// Encode fpgmu as NAN.
			encode_nan(fpgmu);
			
			return;
		}
		
		// The series of IFs below are for multiplying any NON-ZERO, NON-NAN value by INF or -INF.
		// NOTE: THIS ALSO INCLUDES MULTIPLYING INF OR -INF BY ITSELF.
		
		// If we multiply INF * INF, return INF.
		if(exp1 > 62 && frac1 == 0.0 && S1 == 0 && exp2 > 62 && frac2 == 0.0 && S2 == 0)
		{
			// Set fpgmu to either source.
			*fpgmu = source1;
			
			return;
		}	
		
		// If we multiply -INF * -INF, return INF.
		if(exp1 > 62 && frac1 == 0.0 && S1 == 1 && exp2 > 62 && frac2 == 0.0 && S2 == 1)
		{
			// Set the sign bit of source1 to 0 (since we need INF).
			source1 &= ~(1 << 14);
			
			// Set fpgmu to source1 (now +INF).
			*fpgmu = source1;
				
			return;
		}
		
		// If we multiply INF * -INF, return -INF.
		// If we multiply -INF * INF, return -INF.
		if(exp1 > 62 && frac1 == 0.0 && (S1 == 0 || S1 == 1) && exp2 > 62 && frac2 == 0.0 && (S2 == 0 || S2 == 1))
		{
			// Regardless of the order, we need to return -INF, so just set the sign bit of source1 to 1.
			source1 |= (1 << 14);
		
			// Set fpgmu to source1 (should be -INF by our doing or not).
			*fpgmu = source1;
			
			return;
		
		}
		
		// If we multiply INF/-INF by some non-NAN, non-INF value (a norm #).
		// Source 1 is INF/-INF.
		if(exp1 > 62 && frac1 == 0.0 && (S1 == 0 || S1 == 1) && exp2 <= 62)
		{
			// Set the sign of INF based on multiplication.
			if(xor_sign == 1) { source1 |= (1 << 14); }		// Set sign bit to 1 (-INF).
			else if(xor_sign == 0) { source1 &= ~(1 << 14); }	// Set sign bit to 0 (+INF).
			
			// Set fpgmu to source1 (INF with the proper sign bit).
			*fpgmu = source1;
			
			return;
		}
		
		// If we multiply some non-NAN, non-INF value (a norm #) by INF/-INF.
		// source2 is INF/-INF.
		if(exp2 > 62 && frac2 == 0.0 && (S2 == 0 || S2 == 1) && exp1 <= 62)
		{
			// Set the sign of INF based on multiplication.
			if(xor_sign == 1) { source2 |= (1 << 14); }		// Set the sign bit to 1 (-INF).
			else if(xor_sign == 0) { source2 &= ~(1 << 14); }	// Set the sign bit to 0 (+INF).
				
			// Set the fpgmu to source2 (INF with proper sign bit).
			*fpgmu = source2;
			
			return;
		}
	}
	
	
	// If either arugment is NaN, we need to return NaN.
	// Being in this function already implies that exp1 and exp2 > 62.
	// The only way to hit this case pretty much is to add -INF or INF (vice versa) and
	// then add the result of that to another number like 2.0.
	if((exp1 > 62 && frac1 != 0) || (exp2 > 62 && frac2 != 0))
	{
		// Call our helper function to encode fpgmu as NaN.
		encode_nan(fpgmu);
		
		return;
	}
	
	// If we add INF and -INF, we need to return NAN.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 0 && exp2 > 62 && frac2 == 0.0 && S2 == 1)
	{
		// Call our helper function to encode fpgmu as NaN.
		encode_nan(fpgmu);
	
		return;
	}
	
	// If we add -INF and INF, we need to return NAN.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 1 && exp2 > 62 && frac2 == 0.0 && S2 == 0)
	{
		// Call our helper function to encode fpgmu as NaN.
		encode_nan(fpgmu);
		
		return;
	}
	
	// If we add any value (non NaN, -INF) to INF, return INF.
	// This means we can also add NORMALIZED or DENORMALIZED to INF, -INF, so we
	// only need to check that exp2 is NOT SPECIALIZED.
	// Take INF to be source1.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 0 && exp2 <= 62)
	{
		// Set fpgmu to source1 (which is INF).
		*fpgmu = source1;
		
		return;
	}

	
	// If we add any value (non NaN, -INF) to INF, return INF.
	// Take INF to be source2.
	if(exp2 > 62 && frac2 == 0.0 && S2 == 0 && exp1 <= 62)
	{
		// Set fpgmu to source2 (which is INF).
		*fpgmu = source2;
		
		return;
	}
	
	// If we add any value (non NaN, INF) to -INF, return -INF.
	// Take -INF to be source1.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 1 && exp2 <= 62)
	{
		// Set fpgmu to source1 (which is -INF).
		*fpgmu = source1;
		
		return;
	
	}
	
	// If we add any value (non NaN, INF) to -INF, return -INF.
	// Take -INF to be source2.
	if(exp2 > 62 && frac2 == 0.0 && S2 == 1 && exp1 <= 62)
	{
		// Set fpgmu to source2 (which in -INF).
		*fpgmu = source2;
			
		return;
	}
	
	// If we add INF and INF, return INF.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 0 && exp2 > 62 && frac2 == 0.0 && S2 == 0)
	{
		// Set fpgmu to either source (since both are INF).
		*fpgmu = source1;
			
		return;
	}
	
	// If we add -INF and -INF, return -INF.
	if(exp1 > 62 && frac1 == 0.0 && S1 == 1 && exp2 > 62 && frac2 == 0.0 && S2 == 1)
	{	
		// Set fpgmu to either source (since both are -INF).
		*fpgmu = source1;
		
		return;
	}
	
	return; 	
}
	
