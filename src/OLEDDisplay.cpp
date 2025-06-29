/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
 * Copyright (c) 2018 by Fabrice Weinberg
 * Copyright (c) 2019 by Helmut Tschemernjak - www.radioshuttle.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ThingPulse invests considerable time and money to develop these open source libraries.
 * Please support us by buying our products (and not the clones) from
 * https://thingpulse.com
 *
 */

 /*
  * TODO Helmut
  * - test/finish dislplay.printf() on mbed-os
  */

#include "OLEDDisplay.h"

OLEDDisplay::OLEDDisplay() {

	displayWidth = 128;
	displayHeight = 64;
	displayBufferSize = displayWidth * displayHeight / 8;
  inhibitDrawLogBuffer = false;
	color = WHITE;
	geometry = GEOMETRY_128_64;
	textAlignment = TEXT_ALIGN_LEFT;
	fontData = ArialMT_Plain_10;
	fontTableLookupFunction = DefaultFontTableLookup;
	buffer = NULL;
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
	buffer_back = NULL;
#endif
}

OLEDDisplay::~OLEDDisplay() {
  end();
}

bool OLEDDisplay::allocateBuffer() {

  logBufferSize = 0;
  logBufferFilled = 0;
  logBufferLine = 0;
  logBufferMaxLines = 0;
  logBuffer = NULL;

  if (!connect()) {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Can't establish connection to display\n");
    return false;
  }

  if(this->buffer==NULL) {
    this->buffer = (uint8_t*) malloc((sizeof(uint8_t) * displayBufferSize) + BufferOffset);
    this->buffer += BufferOffset;

    if(!this->buffer) {
      DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create display\n");
      return false;
    }
  }

  #ifdef OLEDDISPLAY_DOUBLE_BUFFER
  if(this->buffer_back==NULL) {
    this->buffer_back = (uint8_t*) malloc((sizeof(uint8_t) * displayBufferSize) + BufferOffset);
    this->buffer_back += BufferOffset;

    if(!this->buffer_back) {
      DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create back buffer\n");
      free(this->buffer - BufferOffset);
      return false;
    }
  }
  #endif

  return true;
}

bool OLEDDisplay::init() {

  BufferOffset = getBufferOffset();

  if(!allocateBuffer()) {
    return false;
  }

  sendInitCommands();
  resetDisplay();

  return true;
}

void OLEDDisplay::end() {
  if (this->buffer) { free(this->buffer - BufferOffset); this->buffer = NULL; }
  #ifdef OLEDDISPLAY_DOUBLE_BUFFER
  if (this->buffer_back) { free(this->buffer_back - BufferOffset); this->buffer_back = NULL; }
  #endif
  if (this->logBuffer != NULL) { free(this->logBuffer); this->logBuffer = NULL; }
}

void OLEDDisplay::resetDisplay(void) {
  clear();
  #ifdef OLEDDISPLAY_DOUBLE_BUFFER
  memset(buffer_back, 1, displayBufferSize);
  #endif
  display();
}

void OLEDDisplay::setColor(OLEDDISPLAY_COLOR color) {
  this->color = color;
}

OLEDDISPLAY_COLOR OLEDDisplay::getColor() {
  return this->color;
}

void OLEDDisplay::setPixel(int16_t x, int16_t y) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case WHITE:   buffer[x + (y / 8) * this->width()] |=  (1 << (y & 7)); break;
      case BLACK:   buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y / 8) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}

void OLEDDisplay::setPixelColor(int16_t x, int16_t y, OLEDDISPLAY_COLOR color) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case WHITE:   buffer[x + (y / 8) * this->width()] |=  (1 << (y & 7)); break;
      case BLACK:   buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y / 8) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}

void OLEDDisplay::clearPixel(int16_t x, int16_t y) {
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
    switch (color) {
      case BLACK:   buffer[x + (y >> 3) * this->width()] |=  (1 << (y & 7)); break;
      case WHITE:   buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7)); break;
      case INVERSE: buffer[x + (y >> 3) * this->width()] ^=  (1 << (y & 7)); break;
    }
  }
}


// Bresenham's algorithm - thx wikipedia and Adafruit_GFX
void OLEDDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      setPixel(y0, x0);
    } else {
      setPixel(x0, y0);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void OLEDDisplay::drawRect(int16_t x, int16_t y, int16_t width, int16_t height) {
  drawHorizontalLine(x, y, width);
  drawVerticalLine(x, y, height);
  drawVerticalLine(x + width - 1, y, height);
  drawHorizontalLine(x, y + height - 1, width);
}

void OLEDDisplay::fillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height) {
  for (int16_t x = xMove; x < xMove + width; x++) {
    drawVerticalLine(x, yMove, height);
  }
}

void OLEDDisplay::drawCircle(int16_t x0, int16_t y0, int16_t radius) {
  int16_t x = 0, y = radius;
	int16_t dp = 1 - radius;
	do {
		if (dp < 0)
			dp = dp + (x++) * 2 + 3;
		else
			dp = dp + (x++) * 2 - (y--) * 2 + 5;

		setPixel(x0 + x, y0 + y);     //For the 8 octants
		setPixel(x0 - x, y0 + y);
		setPixel(x0 + x, y0 - y);
		setPixel(x0 - x, y0 - y);
		setPixel(x0 + y, y0 + x);
		setPixel(x0 - y, y0 + x);
		setPixel(x0 + y, y0 - x);
		setPixel(x0 - y, y0 - x);

	} while (x < y);

  setPixel(x0 + radius, y0);
  setPixel(x0, y0 + radius);
  setPixel(x0 - radius, y0);
  setPixel(x0, y0 - radius);
}

void OLEDDisplay::drawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads) {
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  while (x < y) {
    if (dp < 0)
      dp = dp + (x++) * 2 + 3;
    else
      dp = dp + (x++) * 2 - (y--) * 2 + 5;
    if (quads & 0x1) {
      setPixel(x0 + x, y0 - y);
      setPixel(x0 + y, y0 - x);
    }
    if (quads & 0x2) {
      setPixel(x0 - y, y0 - x);
      setPixel(x0 - x, y0 - y);
    }
    if (quads & 0x4) {
      setPixel(x0 - y, y0 + x);
      setPixel(x0 - x, y0 + y);
    }
    if (quads & 0x8) {
      setPixel(x0 + x, y0 + y);
      setPixel(x0 + y, y0 + x);
    }
  }
  if (quads & 0x1 && quads & 0x8) {
    setPixel(x0 + radius, y0);
  }
  if (quads & 0x4 && quads & 0x8) {
    setPixel(x0, y0 + radius);
  }
  if (quads & 0x2 && quads & 0x4) {
    setPixel(x0 - radius, y0);
  }
  if (quads & 0x1 && quads & 0x2) {
    setPixel(x0, y0 - radius);
  }
}


void OLEDDisplay::fillCircle(int16_t x0, int16_t y0, int16_t radius) {
  int16_t x = 0, y = radius;
	int16_t dp = 1 - radius;
	do {
		if (dp < 0)
      dp = dp + (x++) * 2 + 3;
    else
      dp = dp + (x++) * 2 - (y--) * 2 + 5;

    drawHorizontalLine(x0 - x, y0 - y, 2*x);
    drawHorizontalLine(x0 - x, y0 + y, 2*x);
    drawHorizontalLine(x0 - y, y0 - x, 2*y);
    drawHorizontalLine(x0 - y, y0 + x, 2*y);


	} while (x < y);
  drawHorizontalLine(x0 - radius, y0, 2 * radius);

}

void OLEDDisplay::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2) {
  drawLine(x0, y0, x1, y1);
  drawLine(x1, y1, x2, y2);
  drawLine(x2, y2, x0, y0);
}

void OLEDDisplay::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2) {
  int16_t a, b, y, last;

  if (y0 > y1) {
    _swap_int16_t(y0, y1);
    _swap_int16_t(x0, x1);
  }
  if (y1 > y2) {
    _swap_int16_t(y2, y1);
    _swap_int16_t(x2, x1);
  }
  if (y0 > y1) {
    _swap_int16_t(y0, y1);
    _swap_int16_t(x0, x1);
  }

  if (y0 == y2) {
    a = b = x0;
    if (x1 < a) {
      a = x1;
    } else if (x1 > b) {
      b = x1;
    }
    if (x2 < a) {
      a = x2;
    } else if (x2 > b) {
      b = x2;
    }
    drawHorizontalLine(a, y0, b - a + 1);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int32_t
    sa   = 0,
    sb   = 0;

  if (y1 == y2) {
    last = y1; // Include y1 scanline
  } else {
    last = y1 - 1; // Skip it
  }

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;

    if (a > b) {
      _swap_int16_t(a, b);
    }
    drawHorizontalLine(a, y, b - a + 1);
  }

  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;

    if (a > b) {
      _swap_int16_t(a, b);
    }
    drawHorizontalLine(a, y, b - a + 1);
  }
}

void OLEDDisplay::drawHorizontalLine(int16_t x, int16_t y, int16_t length) {
  if (y < 0 || y >= this->height()) { return; }

  if (x < 0) {
    length += x;
    x = 0;
  }

  if ( (x + length) > this->width()) {
    length = (this->width() - x);
  }

  if (length <= 0) { return; }

  uint8_t * bufferPtr = buffer;
  bufferPtr += (y >> 3) * this->width();
  bufferPtr += x;

  uint8_t drawBit = 1 << (y & 7);

  switch (color) {
    case WHITE:   while (length--) {
        *bufferPtr++ |= drawBit;
      }; break;
    case BLACK:   drawBit = ~drawBit;   while (length--) {
        *bufferPtr++ &= drawBit;
      }; break;
    case INVERSE: while (length--) {
        *bufferPtr++ ^= drawBit;
      }; break;
  }
}

void OLEDDisplay::drawVerticalLine(int16_t x, int16_t y, int16_t length) {
  if (x < 0 || x >= this->width()) return;

  if (y < 0) {
    length += y;
    y = 0;
  }

  if ( (y + length) > this->height()) {
    length = (this->height() - y);
  }

  if (length <= 0) return;


  uint8_t yOffset = y & 7;
  uint8_t drawBit;
  uint8_t *bufferPtr = buffer;

  bufferPtr += (y >> 3) * this->width();
  bufferPtr += x;

  if (yOffset) {
    yOffset = 8 - yOffset;
    drawBit = ~(0xFF >> (yOffset));

    if (length < yOffset) {
      drawBit &= (0xFF >> (yOffset - length));
    }

    switch (color) {
      case WHITE:   *bufferPtr |=  drawBit; break;
      case BLACK:   *bufferPtr &= ~drawBit; break;
      case INVERSE: *bufferPtr ^=  drawBit; break;
    }

    if (length < yOffset) return;

    length -= yOffset;
    bufferPtr += this->width();
  }

  if (length >= 8) {
    switch (color) {
      case WHITE:
      case BLACK:
        drawBit = (color == WHITE) ? 0xFF : 0x00;
        do {
          *bufferPtr = drawBit;
          bufferPtr += this->width();
          length -= 8;
        } while (length >= 8);
        break;
      case INVERSE:
        do {
          *bufferPtr = ~(*bufferPtr);
          bufferPtr += this->width();
          length -= 8;
        } while (length >= 8);
        break;
    }
  }

  if (length > 0) {
    drawBit = (1 << (length & 7)) - 1;
    switch (color) {
      case WHITE:   *bufferPtr |=  drawBit; break;
      case BLACK:   *bufferPtr &= ~drawBit; break;
      case INVERSE: *bufferPtr ^=  drawBit; break;
    }
  }
}

void OLEDDisplay::drawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = 2 * radius;
  uint16_t innerRadius = radius - 2;

  setColor(WHITE);
  drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
  drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

  uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 100;

  fillCircle(xRadius, yRadius, innerRadius);
  fillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
  fillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
}

void OLEDDisplay::drawFastImage(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *image) {
  drawInternal(xMove, yMove, width, height, reinterpret_cast<const uint16_t*>(image), 0, 0);
}

void OLEDDisplay::drawXbm(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *xbm) {
  int16_t widthInXbm = (width + 7) / 8;
  uint8_t data = 0;

  for(int16_t y = 0; y < height; y++) {
    for(int16_t x = 0; x < width; x++ ) {
      if (x & 7) {
        data >>= 1; // Move a bit
      } else {  // Read new data every 8 bit
        data = pgm_read_byte(xbm + (x / 8) + y * widthInXbm);
      }
      // if there is a bit draw it
      if (data & 0x01) {
        setPixel(xMove + x, yMove + y);
      }
    }
  }
}

void OLEDDisplay::drawIco16x16(int16_t xMove, int16_t yMove, const uint8_t *ico, bool inverse) {
  uint16_t data;

  for(int16_t y = 0; y < 16; y++) {
    data = pgm_read_byte(ico + (y << 1)) + (pgm_read_byte(ico + (y << 1) + 1) << 8);
    for(int16_t x = 0; x < 16; x++ ) {
      if ((data & 0x01) ^ inverse) {
        setPixelColor(xMove + x, yMove + y, WHITE);
      } else {
        setPixelColor(xMove + x, yMove + y, BLACK);
      }
      data >>= 1; // Move a bit
    }
  }
}

uint16_t OLEDDisplay::drawStringInternal(int16_t xMove, int16_t yMove, const char* text, uint16_t textLength, uint16_t textWidth, bool utf8) {
  uint16_t textHeight       = pgm_read_word(fontData + HEIGHT_POS);
  uint16_t firstChar        = pgm_read_word(fontData + FIRST_CHAR_POS);
  uint16_t sizeOfJumpTable = pgm_read_word(fontData + CHAR_NUM_POS)  * JUMPTABLE_BYTES;

  uint16_t cursorX         = 0;
  uint16_t cursorY         = 0;
  uint16_t charCount       = 0;

  switch (textAlignment) {
    case TEXT_ALIGN_CENTER_BOTH:
      yMove -= textHeight >> 1;
    // Fallthrough
    case TEXT_ALIGN_CENTER:
      xMove -= textWidth >> 1; // divide by 2
      break;
    case TEXT_ALIGN_RIGHT:
      xMove -= textWidth;
      break;
    case TEXT_ALIGN_LEFT:
      break;
  }

  // Don't draw anything if it is not on the screen.
  if (xMove + textWidth  < 0 || xMove >= this->width() ) {return 0;}
  if (yMove + textHeight < 0 || yMove >= this->height()) {return 0;}

  for (uint16_t j = 0; j < textLength; j++) {
    int16_t xPos = xMove + cursorX;
    int16_t yPos = yMove + cursorY;
    if (xPos > this->width())
      break; // no need to continue
    charCount++;

    uint8_t code;
    if (utf8) {
      code = (this->fontTableLookupFunction)(text[j]);
      if (code == 0)
        continue;
    } else
      code = text[j];
    if (code >= firstChar) {
      uint8_t charCode = code - firstChar;

      // 4 Bytes per char code
      uint16_t msbJumpToChar    = pgm_read_word( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES );                  // MSB  \ JumpAddress
      uint16_t lsbJumpToChar    = pgm_read_word( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);   // LSB /
      uint16_t charByteSize     = pgm_read_word( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);  // Size
      uint16_t currentCharWidth = pgm_read_word( fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH); // Width

      // Test if the char is drawable
      if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
        // Get the position of the char data
        uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);
        drawInternal(xPos, yPos, currentCharWidth, textHeight, fontData, charDataPosition, charByteSize);
      }

      cursorX += currentCharWidth;
    }
  }
  return charCount;
}


uint16_t OLEDDisplay::drawString(int16_t xMove, int16_t yMove, const String &strUser) {
  uint16_t lineHeight = pgm_read_word(fontData + HEIGHT_POS);

  // char* text must be freed!
  char* text = strdup(strUser.c_str());
  if (!text) {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][drawString] Can't allocate char array.\n");
    return 0;
  }

  uint16_t yOffset = 0;
  // If the string should be centered vertically too
  // we need to now how heigh the string is.
  if (textAlignment == TEXT_ALIGN_CENTER_BOTH) {
    uint16_t lb = 0;
    // Find number of linebreaks in text
    for (uint16_t i=0;text[i] != 0; i++) {
      lb += (text[i] == 10);
    }
    // Calculate center
    yOffset = (lb * lineHeight) / 2;
  }

  uint16_t charDrawn = 0;
  uint16_t line = 0;
  char* textPart = strtok(text,"\n");
  while (textPart != NULL) {
    uint16_t length = strlen(textPart);
    charDrawn += drawStringInternal(xMove, yMove - yOffset + (line++) * lineHeight, textPart, length, getStringWidth(textPart, length, true), true);
    textPart = strtok(NULL, "\n");
  }
  free(text);
  return charDrawn;
}

void OLEDDisplay::drawStringf( int16_t x, int16_t y, char* buffer, String format, ... )
{
  va_list myargs;
  va_start(myargs, format);
  vsprintf(buffer, format.c_str(), myargs);
  va_end(myargs);
  drawString( x, y, buffer );
}

uint16_t OLEDDisplay::drawStringMaxWidth(int16_t xMove, int16_t yMove, uint16_t maxLineWidth, const String &strUser) {
  uint16_t firstChar  = pgm_read_word(fontData + FIRST_CHAR_POS);
  uint16_t lineHeight = pgm_read_word(fontData + HEIGHT_POS);

  const char* text = strUser.c_str();

  uint16_t length = strlen(text);
  uint16_t lastDrawnPos = 0;
  uint16_t lineNumber = 0;
  uint16_t strWidth = 0;

  uint16_t preferredBreakpoint = 0;
  uint16_t widthAtBreakpoint = 0;
  uint16_t firstLineChars = 0;
  uint16_t drawStringResult = 1; // later tested for 0 == error, so initialize to 1

  for (uint16_t i = 0; i < length; i++) {
    char c = (this->fontTableLookupFunction)(text[i]);
    if (c == 0)
      continue;
    strWidth += pgm_read_word(fontData + JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

    // Always try to break on a space, dash or slash
    if (text[i] == ' ' || text[i]== '-' || text[i] == '/') {
      preferredBreakpoint = i + 1;
      widthAtBreakpoint = strWidth;
    }

    if (strWidth >= maxLineWidth) {
      if (preferredBreakpoint == 0) {
        preferredBreakpoint = i;
        widthAtBreakpoint = strWidth;
      }
      drawStringResult = drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight , &text[lastDrawnPos], preferredBreakpoint - lastDrawnPos, widthAtBreakpoint, true);
      if (firstLineChars == 0)
        firstLineChars = preferredBreakpoint;
      lastDrawnPos = preferredBreakpoint;
      // It is possible that we did not draw all letters to i so we need
      // to account for the width of the chars from `i - preferredBreakpoint`
      // by calculating the width we did not draw yet.
      strWidth = strWidth - widthAtBreakpoint;
      preferredBreakpoint = 0;
      if (drawStringResult == 0) // we are past the display already?
        break;
    }
  }

  // Draw last part if needed
  if (drawStringResult != 0 && lastDrawnPos < length) {
    drawStringResult = drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight , &text[lastDrawnPos], length - lastDrawnPos, getStringWidth(&text[lastDrawnPos], length - lastDrawnPos, true), true);
  }

  if (drawStringResult == 0 || (yMove + lineNumber * lineHeight) >= this->height()) // text did not fit on screen
    return firstLineChars;
  return 0; // everything was drawn
}

uint16_t OLEDDisplay::getStringWidth(const char* text, uint16_t length, bool utf8) {
  uint16_t firstChar        = pgm_read_word(fontData + FIRST_CHAR_POS);

  uint16_t stringWidth = 0;
  uint16_t maxWidth = 0;

  for (uint16_t i = 0; i < length; i++) {
    char c = text[i];
    if (utf8) {
      c = (this->fontTableLookupFunction)(c);
      if (c == 0)
        continue;
    }
    stringWidth += pgm_read_word(fontData + JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);
    if (c == 10) {
      maxWidth = max(maxWidth, stringWidth);
      stringWidth = 0;
    }
  }

  return max(maxWidth, stringWidth);
}

uint16_t OLEDDisplay::getStringWidth(const String &strUser) {
  uint16_t width = getStringWidth(strUser.c_str(), strUser.length());
  return width;
}

void OLEDDisplay::setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT textAlignment) {
  this->textAlignment = textAlignment;
}

void OLEDDisplay::setFont(const uint16_t *fontData) {
  this->fontData = fontData;
  // New font, so must recalculate. Whatever was there is gone at next print.
  setLogBuffer();
}

void OLEDDisplay::setFont(const char *fontData) {
  setFont(reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(reinterpret_cast<const void*>(fontData))));
}

void OLEDDisplay::displayOn(void) {
  sendCommand(DISPLAYON);
}

void OLEDDisplay::displayOff(void) {
  sendCommand(DISPLAYOFF);
}

void OLEDDisplay::invertDisplay(void) {
  sendCommand(INVERTDISPLAY);
}

void OLEDDisplay::normalDisplay(void) {
  sendCommand(NORMALDISPLAY);
}

void OLEDDisplay::setContrast(uint8_t contrast, uint8_t precharge, uint8_t comdetect) {
  sendCommand(SETPRECHARGE); //0xD9
  sendCommand(precharge); //0xF1 default, to lower the contrast, put 1-1F
  sendCommand(SETCONTRAST);
  sendCommand(contrast); // 0-255
  sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  sendCommand(comdetect);	//0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(DISPLAYON);
}

void OLEDDisplay::setBrightness(uint8_t brightness) {
  uint8_t contrast = brightness;
  if (brightness < 128) {
    // Magic values to get a smooth/ step-free transition
    contrast = brightness * 1.171;
  } else {
    contrast = brightness * 1.171 - 43;
  }

  uint8_t precharge = 241;
  if (brightness == 0) {
    precharge = 0;
  }
  uint8_t comdetect = brightness / 8;

  setContrast(contrast, precharge, comdetect);
}

void OLEDDisplay::resetOrientation() {
  sendCommand(SEGREMAP);
  sendCommand(COMSCANINC);           //Reset screen rotation or mirroring
}

void OLEDDisplay::flipScreenVertically() {
  sendCommand(SEGREMAP | 0x01);
  sendCommand(COMSCANDEC);           //Rotate screen 180 Deg
}

void OLEDDisplay::mirrorScreen() {
  sendCommand(SEGREMAP);
  sendCommand(COMSCANDEC);           //Mirror screen
}

void OLEDDisplay::clear(void) {
  memset(buffer, 0, displayBufferSize);
}

void OLEDDisplay::drawLogBuffer(uint16_t xMove, uint16_t yMove) {
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
  Serial.println("[deprecated] Print functionality now handles buffer management automatically. This is a no-op.");
#endif
}

void OLEDDisplay::drawLogBuffer() {
  uint16_t lineHeight = pgm_read_word(fontData + HEIGHT_POS);
  // Always align left
  setTextAlignment(TEXT_ALIGN_LEFT);

  // State values
  uint16_t length   = 0;
  uint16_t line     = 0;
  uint16_t lastPos  = 0;

  // If the lineHeight and the display height are not cleanly divisible, we need
  // to start off the screen when the buffer has logBufferMaxLines so that the
  // first line, and not the last line, drops off.
  uint16_t shiftUp = (this->logBufferLine == this->logBufferMaxLines) ? (lineHeight - (displayHeight % lineHeight)) % lineHeight : 0;

  for (uint16_t i=0;i<this->logBufferFilled;i++){
    length++;
    // Everytime we have a \n print
    if (this->logBuffer[i] == 10) {
      // Draw string on line `line` from lastPos to length
      // Passing 0 as the lenght because we are in TEXT_ALIGN_LEFT
      drawStringInternal(0, 0 - shiftUp + (line++) * lineHeight, &this->logBuffer[lastPos], length, 0, false);
      // Remember last pos
      lastPos = i;
      // Reset length
      length = 0;
    }
  }
  // Draw the remaining string
  if (length > 0) {
    drawStringInternal(0, 0 - shiftUp + line * lineHeight, &this->logBuffer[lastPos], length, 0, false);
  }
}

uint16_t OLEDDisplay::getWidth(void) {
  return displayWidth;
}

uint16_t OLEDDisplay::getHeight(void) {
  return displayHeight;
}

void OLEDDisplay::cls() {
  clear();
  this->logBufferFilled = 0;
  this->logBufferLine = 0;
  display();
}

bool OLEDDisplay::setLogBuffer(uint16_t lines, uint16_t chars) {
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
  Serial.println("[deprecated] Print functionality now handles buffer management automatically. This is a no-op.");
#endif
  return true;
}

bool OLEDDisplay::setLogBuffer(){
  // don't know how big we need it without a font set.
  if (!fontData)
		return false;
  
  // we're always starting over
  if (logBuffer != NULL)
    free(logBuffer);

  // figure out how big it needs to be
  uint16_t textHeight = pgm_read_word(fontData + HEIGHT_POS);
  if (!textHeight)
    return false;  // Prevent division by zero crashes
  uint16_t lines =  this->displayHeight / textHeight + (this->displayHeight % textHeight ? 1 : 0);
  uint16_t chars =   5 * (this->displayWidth / textHeight);
  uint16_t size = lines * (chars + 1);  // +1 is for \n

  // Something weird must have happened
  if (size == 0) 
    return false;

  // All good, initialize logBuffer
  this->logBufferLine     = 0;      // Lines printed
  this->logBufferFilled   = 0;      // Nothing stored yet
  this->logBufferMaxLines = lines;  // Lines max printable
  this->logBufferLineLen  = chars;  // Chars per line
  this->logBufferSize     = size;   // Total number of characters the buffer can hold
  this->logBuffer         = (char *) malloc(size * sizeof(uint8_t));
  if(!this->logBuffer) {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][setLogBuffer] Not enough memory to create log buffer\n");
    return false;
  }

  return true;
}

size_t OLEDDisplay::write(uint8_t c) {
  if (!fontData)
		return 1;
    
  // Create a logBuffer if there isn't one
	if (!logBufferSize) {
    // Give up if we can't create a logBuffer somehow
		if (!setLogBuffer())
      return 1;
	}

  // Don't waste space on \r\n line endings, dropping \r
  if (c == 13) return 1;

  // convert UTF-8 character to font table index
  c = (this->fontTableLookupFunction)(c);
  // drop unknown character
  if (c == 0) return 1;

  bool maxLineReached = this->logBufferLine >= this->logBufferMaxLines;
  bool bufferFull = this->logBufferFilled >= this->logBufferSize;

  // Can we write to the buffer? If not, make space.
  if (bufferFull || maxLineReached) {
    // See if we can chop off the first line
    uint16_t firstLineEnd = 0;
    for (uint16_t i = 0; i < this->logBufferFilled; i++) {
      if (this->logBuffer[i] == 10){
        // Include last char too
        firstLineEnd = i + 1;
        // Calculate the new logBufferFilled value
        this->logBufferFilled = logBufferFilled - firstLineEnd;
        // Now move other lines to front of the buffer
        memcpy(this->logBuffer, &this->logBuffer[firstLineEnd], logBufferFilled);
        // And voila, buffer one line shorter
        this->logBufferLine--;
        break;
      }
    }
    // In we can't take off first line, we just empty the buffer
    if (!firstLineEnd) {
      this->logBufferFilled = 0;
      this->logBufferLine = 0;
    }
  }

  // So now we know for sure we have space in the buffer

  // Find the length of the last line
  uint16_t lastLineLen= 0;
  for (uint16_t i = 0; i < this->logBufferFilled; i++) {
    lastLineLen++;
    if (this->logBuffer[i] == 10) lastLineLen = 0;
  }
  // if last line is max length, ignore anything but linebreaks
  if (lastLineLen >= this->logBufferLineLen) {
    if (c != 10) return 1;
  }

  // Write to buffer
  this->logBuffer[this->logBufferFilled++] = c;
  // Keep track of lines written
  if (c == 10) this->logBufferLine++;

  // Draw to screen unless we're writing a whole string at a time
  if (!this->inhibitDrawLogBuffer) {
    clear();
    drawLogBuffer();
    display();
  }

  // We always claim we printed it all
  return 1;
}

size_t OLEDDisplay::write(const char* str) {
  if (str == NULL) return 0;
  size_t length = strlen(str);
  // If we write a string, only do the drawLogBuffer at the end, not every time we write a char
  this->inhibitDrawLogBuffer = true;
  for (size_t i = 0; i < length; i++) {
    write(str[i]);
  }
  this->inhibitDrawLogBuffer = false;
  clear();
  drawLogBuffer();
  display();
  return length;
}

#ifdef __MBED__
int OLEDDisplay::_putc(int c) {
	return this->write((uint8_t)c);
}
#endif

// Private functions
void OLEDDisplay::setGeometry(OLEDDISPLAY_GEOMETRY g, uint16_t width, uint16_t height) {
  this->geometry = g;

  switch (g) {
    case GEOMETRY_128_64:
      this->displayWidth = 128;
      this->displayHeight = 64;
      break;
    case GEOMETRY_128_32:
      this->displayWidth = 128;
      this->displayHeight = 32;
      break;
    case GEOMETRY_64_48:
      this->displayWidth = 64;
      this->displayHeight = 48;
      break;
    case GEOMETRY_64_32:
      this->displayWidth = 64;
      this->displayHeight = 32;
      break;
    case GEOMETRY_RAWMODE:
      this->displayWidth = width > 0 ? width : 128;
      this->displayHeight = height > 0 ? height : 64;
      break;
  }
  this->displayBufferSize = displayWidth * displayHeight / 8;
}

void OLEDDisplay::sendInitCommands(void) {
  if (geometry == GEOMETRY_RAWMODE)
  	return;
  sendCommand(DISPLAYOFF);
  sendCommand(SETDISPLAYCLOCKDIV);
  sendCommand(0xF0); // Increase speed of the display max ~96Hz
  sendCommand(SETMULTIPLEX);
  sendCommand(this->height() - 1);
  sendCommand(SETDISPLAYOFFSET);
  sendCommand(0x00);
  if(geometry == GEOMETRY_64_32)
    sendCommand(0x00);
  else
    sendCommand(SETSTARTLINE);
  sendCommand(CHARGEPUMP);
  sendCommand(0x14);
  sendCommand(MEMORYMODE);
  sendCommand(0x00);
  sendCommand(SEGREMAP);
  sendCommand(COMSCANINC);
  sendCommand(SETCOMPINS);

  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32) {
    sendCommand(0x12);
  } else if (geometry == GEOMETRY_128_32) {
    sendCommand(0x02);
  }

  sendCommand(SETCONTRAST);

  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32) {
    sendCommand(0xCF);
  } else if (geometry == GEOMETRY_128_32) {
    sendCommand(0x8F);
  }

  sendCommand(SETPRECHARGE);
  sendCommand(0xF1);
  sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  sendCommand(0x40);	        //0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(0x2e);            // stop scroll
  sendCommand(DISPLAYON);
}

void inline OLEDDisplay::drawInternal(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint16_t *data, uint16_t offset, uint16_t bytesInData) {
  if (width < 0 || height < 0) return;
  if (yMove + height < 0 || yMove > this->height())  return;
  if (xMove + width  < 0 || xMove > this->width())   return;

  uint8_t  rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
  int8_t   yOffset      = yMove & 7;

  bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

  int16_t initYMove   = yMove;
  int8_t  initYOffset = yOffset;


  for (uint16_t i = 0; i < bytesInData; i++) {

    // Reset if next horizontal drawing phase is started.
    if ( i % rasterHeight == 0) {
      yMove   = initYMove;
      yOffset = initYOffset;
    }

    uint16_t currentByte =  pgm_read_word(data + offset + i);

    int16_t xPos = xMove + (i / rasterHeight);
    int16_t yPos = ((yMove >> 3) + (i % rasterHeight)) * this->width();

//    int16_t yScreenPos = yMove + yOffset;
    int16_t dataPos    = xPos  + yPos;

    if (dataPos >=  0  && dataPos < displayBufferSize &&
        xPos    >=  0  && xPos    < this->width() ) {

      if (yOffset >= 0) {
        switch (this->color) {
          case WHITE:   buffer[dataPos] |= currentByte << yOffset; break;
          case BLACK:   buffer[dataPos] &= ~(currentByte << yOffset); break;
          case INVERSE: buffer[dataPos] ^= currentByte << yOffset; break;
        }

        if (dataPos < (displayBufferSize - this->width())) {
          switch (this->color) {
            case WHITE:   buffer[dataPos + this->width()] |= currentByte >> (8 - yOffset); break;
            case BLACK:   buffer[dataPos + this->width()] &= ~(currentByte >> (8 - yOffset)); break;
            case INVERSE: buffer[dataPos + this->width()] ^= currentByte >> (8 - yOffset); break;
          }
        }
      } else {
        // Make new offset position
        yOffset = -yOffset;

        switch (this->color) {
          case WHITE:   buffer[dataPos] |= currentByte >> yOffset; break;
          case BLACK:   buffer[dataPos] &= ~(currentByte >> yOffset); break;
          case INVERSE: buffer[dataPos] ^= currentByte >> yOffset; break;
        }

        // Prepare for next iteration by moving one block up
        yMove -= 8;

        // and setting the new yOffset
        yOffset = 8 - yOffset;
      }
#ifndef __MBED__
      yield();
#endif
    }
  }
}

// You need to free the char!
char* OLEDDisplay::utf8ascii(const String &str) {
  uint16_t k = 0;
  uint16_t length = str.length() + 1;

  // Copy the string into a char array
  char* s = (char*) malloc(length * sizeof(char));
  if(!s) {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][utf8ascii] Can't allocate another char array. Drop support for UTF-8.\n");
    return (char*) str.c_str();
  }
  str.toCharArray(s, length);

  length--;

  for (uint16_t i=0; i < length; i++) {
    char c = (this->fontTableLookupFunction)(s[i]);
    if (c!=0) {
      s[k++]=c;
    }
  }

  s[k]=0;

  // This will leak 's' be sure to free it in the calling function.
  return s;
}

void OLEDDisplay::setFontTableLookupFunction(FontTableLookupFunction function) {
  this->fontTableLookupFunction = function;
}


char DefaultFontTableLookup(const uint8_t ch) {
    // UTF-8 to font table index converter
    // Code form http://playground.arduino.cc/Main/Utf8ascii
	static uint8_t LASTCHAR;

	if (ch < 128) { // Standard ASCII-set 0..0x7F handling
		LASTCHAR = 0;
		return ch;
	}

	uint8_t last = LASTCHAR;   // get last char
	LASTCHAR = ch;

	switch (last) {    // conversion depnding on first UTF8-character
		case 0xC2: return (uint8_t) ch;
		case 0xC3: return (uint8_t) (ch | 0xC0);
		case 0x82: if (ch == 0xAC) return (uint8_t) 0x80;    // special case Euro-symbol
	}

	return (uint8_t) 0; // otherwise: return zero, if character has to be ignored
}
