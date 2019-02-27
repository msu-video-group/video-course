#include <memory>

#include "half_pixel.hpp"

void HalfpixelShift(uint8_t *field, int width, int height, bool shift_up)
{
	int i, j, width2 = width * 2, temp;
	std::unique_ptr<uint8_t[]> new_field(new uint8_t[width*height]);
	uint8_t *old_ptr = field, *new_ptr = new_field.get();
	old_ptr += width;
	for (j = 0; j < width; j++)
	{
		temp = (old_ptr[j-width] + old_ptr[j])>>1;
		if (temp < 255)
			if (temp >= 0);
			else
				temp = 0;
		else
			temp = 255;
		new_ptr[j] = static_cast<uint8_t>(temp);
	}
	old_ptr += width;
	new_ptr += width;
	for (i = 2; i < height - 1; i++)
	{
		for (j = 0; j < width; j++)
		{
			temp = (5*(old_ptr[j-width] + old_ptr[j]) -
				(old_ptr[j-width2] + old_ptr[j+width]))>>3;
			if (temp < 255)
				if (temp >= 0);
				else
					temp = 0;
			else
				temp = 255;
			new_ptr[j] = static_cast<uint8_t>(temp);
		}
		old_ptr += width;
		new_ptr += width;
	}
	for (j = 0; j < width; j++)
	{
		temp = (old_ptr[j-width] + old_ptr[j])>>1;
		if (temp < 255)
			if (temp >= 0);
			else
				temp = 0;
		else
			temp = 255;
		new_ptr[j] = static_cast<uint8_t>(temp);
	}
	if (shift_up)
	{
		temp = width;
	}
	else
	{
		temp = 0;
	}
	memcpy(field + temp, new_field.get(), width * (height - 1) * sizeof(uint8_t));
}

void HalfpixelShiftHorz(uint8_t *field, int width, int height, bool shift_right)
{
	int i, j, temp;
	std::unique_ptr<uint8_t[]> new_field(new uint8_t[width*height]);
	uint8_t *old_ptr = field, *new_ptr = new_field.get();

	old_ptr += 1;
	for (j = 0; j < height; j++)
	{
		temp = (old_ptr[j*width - 1] + old_ptr[j*width])>>1;
		if (temp < 255)
			if (temp >= 0);
			else
				temp = 0;
		else
			temp = 255;
		new_ptr[j*width] = static_cast<uint8_t>(temp);
	}
	old_ptr += 1;
	new_ptr += 1;
	for (i = 2; i < width - 1; i++)
	{
		for (j = 0; j < height; j++)
		{
			temp = (5*(old_ptr[j*width-1] + old_ptr[j*width]) -
				(old_ptr[j*width-2] + old_ptr[j*width+1]))>>3;
			if (temp < 255)
				if (temp >= 0);
				else
					temp = 0;
			else
				temp = 255;
			new_ptr[j*width] = static_cast<uint8_t>(temp);
		}
		old_ptr += 1;
		new_ptr += 1;
	}
	for (j = 0; j < height; j++)
	{
		temp = (old_ptr[j*width-1] + old_ptr[j*width])>>1;
		if (temp < 255)
			if (temp >= 0);
			else
				temp = 0;
		else
			temp = 255;
		new_ptr[j*width] = static_cast<uint8_t>(temp);
	}
	if (shift_right)
	{
		new_ptr = new_field.get();
		old_ptr = field;
		for (j = 0; j < height; j++)
		{
			new_ptr[j*width] = old_ptr[j*width];
		}
	}
	else
	{
		new_ptr += 1;
		for (j = 0; j < height; j++)
		{
			new_ptr[j*width] = old_ptr[j*width];
		}
	}
	memcpy(field, new_field.get(), width * height * sizeof(uint8_t));
}

void HalfpixelShift(int16_t *field, int width, int height, bool shift_up)
{
	int i, j, width2 = width * 2, temp;
	std::unique_ptr<int16_t[]> new_field(new int16_t[width*height]);
	int16_t *old_ptr = field, *new_ptr = new_field.get();
	old_ptr += width;
	for (j = 0; j < width; j++)
	{
		temp = (old_ptr[j-width] + old_ptr[j])>>1;
		new_ptr[j] = static_cast<int16_t>(temp);
	}
	old_ptr += width;
	new_ptr += width;
	for (i = 2; i < height - 1; i++)
	{
		for (j = 0; j < width; j++)
		{
			temp = (5*(old_ptr[j-width] + old_ptr[j]) -
				(old_ptr[j-width2] + old_ptr[j+width]))>>3;
			new_ptr[j] = static_cast<int16_t>(temp);
		}
		old_ptr += width;
		new_ptr += width;
	}
	for (j = 0; j < width; j++)
	{
		temp = (old_ptr[j-width] + old_ptr[j])>>1;
		new_ptr[j] = static_cast<int16_t>(temp);
	}
	if (shift_up)
	{
		temp = width;
	}
	else
	{
		temp = 0;
	}
	memcpy(field + temp, new_field.get(), width * (height - 1) * sizeof(int16_t));
}

void HalfpixelShiftHorz(int16_t *field, int width, int height, bool shift_right)
{
	int i, j, temp;
	std::unique_ptr<int16_t[]> new_field(new int16_t[width*height]);
	int16_t *old_ptr = field, *new_ptr = new_field.get();

	old_ptr += 1;
	for (j = 0; j < height; j++)
	{
		temp = (old_ptr[j*width - 1] + old_ptr[j*width])>>1;
		new_ptr[j*width] = static_cast<int16_t>(temp);
	}
	old_ptr += 1;
	new_ptr += 1;
	for (i = 2; i < width - 1; i++)
	{
		for (j = 0; j < height; j++)
		{
			temp = (5*(old_ptr[j*width-1] + old_ptr[j*width]) -
				(old_ptr[j*width-2] + old_ptr[j*width+1]))>>3;
			new_ptr[j*width] = static_cast<int16_t>(temp);
		}
		old_ptr += 1;
		new_ptr += 1;
	}
	for (j = 0; j < height; j++)
	{
		temp = (old_ptr[j*width-1] + old_ptr[j*width])>>1;
		new_ptr[j*width] = static_cast<int16_t>(temp);
	}
	if (shift_right)
	{
		new_ptr = new_field.get();
		old_ptr = field;
		for (j = 0; j < height; j++)
		{
			new_ptr[j*width] = old_ptr[j*width];
		}
	}
	else
	{
		new_ptr += 1;
		for (j = 0; j < height; j++)
		{
			new_ptr[j*width] = old_ptr[j*width];
		}
	}
	memcpy(field, new_field.get(), width * height * sizeof(int16_t));
}
