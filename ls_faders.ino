/******************************* ls_faders: LinnStrument CC Faders ********************************
This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
***************************************************************************************************
These functions handle the CC faders for each split
**************************************************************************************************/

#define CC_FADER_NUMBER_OFFSET 1

void handleFaderTouch(boolean newVelocity) {
  if (sensorCell->velocity) {
    byte faderLeft, faderLength;
    determineFaderBoundaries(sensorSplit, faderLeft, faderLength);
    handleFaderTouch(newVelocity, faderLeft, faderLength);
  }
}

void handleFaderTouch(boolean newVelocity, byte faderLeft, byte faderLength) {
  if (sensorCell->velocity) {
    unsigned short ccForFader = Split[sensorSplit].ccForFader[sensorRow];
    if (ccForFader > 128) return;

    // only proceed when this is the touch on the highest row in the same split when the CC numbers
    // are the same, only one fader with the same cc number can be used at a time
    for (byte r = 7; r > sensorRow; --r) {
      if (Split[sensorSplit].ccForFader[r] == ccForFader && hasTouchInSplitOnRow(sensorSplit, r)) {
        return;
      }
    }

    short value = -1;

    // when the fader only spans one cell, it acts as a toggle
    if (faderLength == 0) {
      if (newVelocity) {
        if (ccFaderValues[sensorSplit][ccForFader] > 0) {
          value = 0;
        }
        else {
          value = 127;
        }
      }
    }
    // otherwise it's a real fader and we calculate the value based on its position
    else {
      // if a new touch happens on the same row that is further down the row, make it
      // take over the touch
      if (newVelocity) {
        for (byte col = faderLength + faderLeft; col >= faderLeft; --col) {
          if (col != sensorCol && cell(col, sensorRow).velocity) {
            transferFromSameRowCell(col);
            return;
          }
        }
      }

      // initialize the initial fader touch
      value = calculateFaderValue(sensorCell->calibratedX(), faderLeft, faderLength);
    }

    if (value >= 0) {
      ccFaderValues[sensorSplit][ccForFader] = value;
      Split[sensorSplit].valForFader[sensorRow] = value;
      sendFaderValue(ccForFader, value);
      paintFadersForSplitMatchingCC(sensorSplit, ccForFader, faderLeft, faderLength);
      if(Global.splitActive) {
        // update low row and all faders with this CC number other visible splits, since they might have changed
        for(byte split = 0; split<NUMSPLITS; split++) {
          if(split != sensorSplit) {
            if(Split[split].ccFaders) {
              paintFadersForSplitMatchingCC(split, ccForFader);
            }
            if (Split[split].lowRowCCXBehavior == lowRowCCFader && Split[split].ccForLowRow==ccForFader) {
              paintLowRowCCX(split);
            }
            
          }
        }
      }
    }
  }
}

void paintFadersForSplitMatchingCC(byte split, byte ccForFader) {
  byte faderLeft;
  byte faderLength;
  determineFaderBoundaries(split, faderLeft, faderLength);
  paintFadersForSplitMatchingCC(split, ccForFader, faderLeft, faderLength);
}

void paintFadersForSplitMatchingCC(byte split, byte ccForFader, byte faderLeft, byte faderLength) {
  for (byte f = 0; f < 8; ++f) {
    if (Split[split].ccForFader[f] == ccForFader) {
      performContinuousTasks();
      paintCCFaderDisplayRow(split, f, faderLeft, faderLength);
    }
  }
}

void sendFaderValue(unsigned short ccForFader, short value)
{
  for(byte split = 0; split<NUMSPLITS; split++) {
    // TODO: check for all matches in Split[split].midiChanSet[]
    if ( (Split[split].midiChanMainEnabled && Split[split].midiChanMain == Split[sensorSplit].midiChanMain) ||
         (Split[split].midiChanSet[Split[sensorSplit].midiChanMain])  )
    {
      if(ccForFader == Split[split].customCCForZ)
      {
        Split[split].ctrForZ = value;
      }

      if(ccForFader == Split[split].customCCForY)
      {
        Split[split].ctrForY = value;
      }
      
      ccFaderValues[split][ccForFader] = value;
      for (byte f = 0; f < 8; ++f) {
        if (Split[split].ccForFader[f] == ccForFader) {
          Split[split].valForFader[f] = value;
        }
      }
    }
  }
  
  preSendControlChange(sensorSplit, ccForFader, value, false);
}

void handleFaderRelease() {
  byte faderLeft, faderLength;
  determineFaderBoundaries(sensorSplit, faderLeft, faderLength);
  handleFaderRelease(faderLeft, faderLength);
}

void handleFaderRelease(byte faderLeft, byte faderLength) {
  // if another touch is already down on the same row, make it take over the touch
  if (sensorCell->velocity) {
    if (faderLength > 0) {
      for (byte col = faderLength + faderLeft; col >= faderLeft; --col) {
        if (col != sensorCol && cell(col, sensorRow).touched == touchedCell) {
          transferToSameRowCell(col);
          break;
        }
      }
    }
  }
}

void determineFaderBoundaries(byte split, byte& faderLeft, byte& faderLength) {
  faderLeft = 1;
  faderLength = NUMCOLS-2;
  if (Global.splitActive || displayMode == displaySplitPoint) {
    if (split == LEFT) {
      faderLength = Global.splitPoint - 2;
    }
    else {
      faderLeft = Global.splitPoint;
      faderLength = NUMCOLS - Global.splitPoint - 1;
    }
  }
}

byte calculateFaderValue(short x, byte faderLeft, byte faderLength) {
  int32_t fxdFaderRange = FXD_MUL(FXD_FROM_INT(faderLength), FXD_CALX_FULL_UNIT);
  int32_t fxdFaderPosition = FXD_FROM_INT(x) - Device.calRows[faderLeft][0].fxdReferenceX;
  int32_t fxdFaderRatio = FXD_DIV(fxdFaderPosition, fxdFaderRange);
  int32_t fxdFaderValue = FXD_MUL(FXD_CONST_127, fxdFaderRatio);
  return constrain(FXD_TO_INT(fxdFaderValue), 0, 127);
}

int32_t fxdCalculateFaderPosition(byte value, byte faderLeft, byte faderLength) {
  return fxdCalculateFaderPosition(value, faderLeft, faderLength, FXD_CONST_127);
}

int32_t fxdCalculateFaderPosition(byte value, byte faderLeft, byte faderLength, int32_t fxdMaxValue) {
  int32_t fxdFaderRange = FXD_MUL(FXD_FROM_INT(faderLength), FXD_CALX_FULL_UNIT);
  int32_t fxdFaderRatio = FXD_DIV(FXD_FROM_INT(value), fxdMaxValue);
  int32_t fxdFaderPosition = FXD_MUL(fxdFaderRange, fxdFaderRatio) + Device.calRows[faderLeft][0].fxdReferenceX;
  return fxdFaderPosition;
}