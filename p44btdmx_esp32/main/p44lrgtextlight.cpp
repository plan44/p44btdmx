//
//  Copyright (c) 2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0

#include "p44lrgtextlight.hpp"
#include "jsonobject.hpp"


using namespace p44;

// MARK: - ledchain scrolling text light

const char *textLightConfig =
  "{'type':'scroller','label':'SCROLLER','dx':113,'dy':7,"
  "'scrolledview':{'type':'text','label':'LIGHT','x':0,'y':0,'sizetocontent':true,'text':' ... ','wrapmode':3},"
  "'offsety':0}";



P44lrgTextLight::P44lrgTextLight(P44ViewPtr aRootView, PixelRect aFrame)
{
  FOCUSLOG("P44lrgTextLight created on mainloop@%p", &MainLoop::currentMainLoop());
  P44ViewPtr baseview;
  ErrorPtr err = createViewFromConfig(JsonObject::objFromText(textLightConfig), baseview, P44ViewPtr());
  OLOG(LOG_INFO, "text view create status: %s", Error::text(err));
  mLightView = dynamic_pointer_cast<TextView>(baseview->getView("LIGHT"));
  mScrollerView = dynamic_pointer_cast<ViewScroller>(baseview->getView("SCROLLER"));
  // add to root view
  aRootView->addSubView(baseview);
  OLOG(LOG_INFO, "view hierarchy: %s", aRootView->viewStatus()->json_c_str());
}

P44lrgTextLight::~P44lrgTextLight()
{
}

const char* texts[] = {
  "Hallo TV-Roboter! +++ ",
  "Space Dream *** ",
  "Des Menschen Seele  Gleicht dem Wasser:  Vom Himmel kommt es,  Zum Himmel steigt es,  Und wieder nieder  Zur Erde muss es.  Ewig wechselnd.  -  Strömt von der hohen,  Steilen Felsenwand  Der reine Strahl,  Dann stäubt er lieblich  In Wolkenwellen  Zum glatten Fels,  Und, leicht empfangen,  Wallt er verschleiernd,  Leisrauschend  Zur Tiefe nieder.  -  Ragen Klippen  Dem Sturz entgegen,  Schäumt er unmutig  Stufenweise  Zum Abgrund.  -  Im flachen Bette  Schleicht er das Wiesental hin,  Und in dem glatten See  Weiden ihr Antlitz  Alle Gestirne. - Wind ist der Welle  Lieblicher Buhler;  Wind rauscht von Grund aus  Schäumende Wogen.  -  Seele des Menschen  Wie gleichst du dem Wasser!  Schicksal des Menschen,  Wie gleichst du dem Wind!               ",
  "NETTO 0 BIS 2030 - Wir fordern netto 0 Treibhausgasemissionen bis 2030, damit die Schweiz nur noch so viel Emissionen ausstösst, wie die Natur aufnehmen kann. (climatestrike.ch) +++ ",
  "The Quick Brown Fox Jumps Over The Lazy Dog ... ",
  "So Long, and Thanks for All the Fish --- ",
  "Azelle, Bölle schäle, d Chatz gaht uf Walliselle, chunnt si wieder hei, hätt si chrummi Bei. Piff, paff, puff, und du bisch ehr und redlich duss."
};
const int numTexts = sizeof(texts)/sizeof(char*);



// light layout: HSB + extras: 8 channels
// 0: channel hue
// 1: channel saturation
// 2: channel brightness
// 3: channel position
// 4: channel text selector (normally: size)
// 5: channel text scrolling speed
// 6: channel effect specific: gradient
// 7: channel mode

void P44lrgTextLight::applyChannels()
{
  uint8_t mode = channels[7].pending;
  bool animationChanged = false;
  // - convert HSV to Pixel
  PixelColor col = hsbToPixel(
    (double)channels[0].pending/255*360,
    (double)channels[1].pending/255,
    (double)channels[2].pending/255,
    true // brightness as alpha, full RGB value
  );
  // check what to update
  if (
    (channels[0].pending!=channels[0].current) || // H
    (channels[1].pending!=channels[1].current) || // S
    (channels[2].pending!=channels[2].current) || // V
    (channels[7].pending!=channels[7].current)    // mode
  ) {
    // need updating RGB outputs
    if (channels[7].pending!=channels[7].current) {
      // mode change, including color
      switch(mode) {
        default:
        case 0: {
          // full size light with hard edges
          mLightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          break;
        }
        case 3: {
          // tunable color gradient
          mLightView->setColoringParameters(col, 0, gradient_none, (double)channels[6].pending/64-2, gradient_curve_lin+gradient_repeat_oscillating, 0, gradient_none, false);
          break;
        }
      }
      OLOG(LOG_INFO,"Mode and/or color change");
      OLOG(LOG_INFO,"- new view hierarchy: %s", mLightView->getParent()->viewStatus()->json_c_str());
    }
    else {
      // just color
      OLOG(LOG_INFO,"Color only change");
      mLightView->setForegroundColor(col);
    }
  }
  // Position selects the text
  if (
    (channels[3].pending!=channels[3].current)
  ) {
    const char *text = " ... ";
    if (channels[3].pending>=1 && channels[3].pending<=numTexts) {
      text = texts[channels[3].pending-1];
    }
    mLightView->setText(text);
    // also re-apply relative extent to cover new contents
    mLightView->setRelativeExtent((double)channels[4].pending/128); // 0..2, so radius (center to edge) can span entire light
  }
  // Size
  if (
    (channels[4].pending!=channels[4].current)
  ) {
    // to make sure light works out of the box, mode 0 actively suppresses size changes!
    if (mode!=0) {
      OLOG(LOG_INFO,"Size change");
      mLightView->setRelativeExtent((double)channels[4].pending/128);
    }
  }
  // Scrolling speed
  if (
    (channels[5].pending!=channels[5].current)
  ) {
    int iv = 255-channels[5].pending;
    if (iv==255) {
      mScrollerView->stopScroll();
    }
    else {
      // four areas of speed with 1/4, 1/3, 1/2 and 1/1 steps, with 20..84mS per step each
      double stepX = 1.0/((iv>>6)+1); // the longer the interval, the smaller the step
      MLMicroSeconds interval = ((iv & 0x3F)+20)*MilliSecond;
      mScrollerView->startScroll(stepX, 0, interval);
    }
  }
  // Gradient/Effect param
  if (
    (channels[6].pending!=channels[6].current) // mode dependent gradient
  ) {
    OLOG(LOG_INFO,"Feature param / Gradient change");
    switch(mode) {
      case 3: {
        // tunable color gradient
        mLightView->setColoringParameters(col, 0, gradient_none, (double)channels[6].pending/128-1, gradient_curve_lin+gradient_repeat_none, 0, gradient_none, false);
        break;
      }
    }
  }
  // request LED update
  FOCUSLOG("will request update, mainloop@%p", &MainLoop::currentMainLoop());
  mLightView->requestUpdateIfNeeded();
  FOCUSLOG("did request update");
  // confirm apply
  inherited::applyChannels();
}

