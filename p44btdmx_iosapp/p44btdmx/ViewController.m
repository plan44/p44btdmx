//
//  ViewController.m
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import "ViewController.h"
#import "SettingsViewController.h"
#import "p44btdmx.h"
#import "AppDelegate.h"

#define LIGHT_CHANNELS 8
#define NUM_LIGHTS 64

@interface ViewController ()
{
  int mCurrentLightNo;
  BOOL mLocallyStopped;
  BOOL mSettingLight;
}
@end


@implementation ViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  // Do any additional setup after loading the view.
  mCurrentLightNo = 0;
  mLocallyStopped = NO;
  mSettingLight = NO;
}


- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  // load settings
  mCurrentLightNo = [[NSUserDefaults standardUserDefaults] integerForKey:@"p44BtDMXcurrentlight"];
  self.lightNo1s.selectedSegmentIndex = mCurrentLightNo % 10;
  self.lightNo10s.selectedSegmentIndex = mCurrentLightNo / 10;
  [self updateChannels];
}


-(void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager startBroadcast];
}


- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager stopBroadcast];
}


- (void)updateLightNoFromControls
{
  mCurrentLightNo =
    (int)self.lightNo10s.selectedSegmentIndex*10 +
    (int)self.lightNo1s.selectedSegmentIndex;
  [[NSUserDefaults standardUserDefaults] setInteger:mCurrentLightNo forKey:@"p44BtDMXcurrentlight"];
}


- (void)updateChannels
{
  P44BTDMXManager *mgr = [AppDelegate sharedAppDelegate].p44BTDMXManager;
  [self showLightInfo];
  int lightBase = mCurrentLightNo*LIGHT_CHANNELS;
  self.hueSlider.value = [mgr.p44BTDMX getChannel:lightBase+0];
  self.saturationSlider.value = [mgr.p44BTDMX getChannel:lightBase+1];
  self.brightnessSlider.value = [mgr.p44BTDMX getChannel:lightBase+2];
  self.positionSlider.value = [mgr.p44BTDMX getChannel:lightBase+3];
  self.sizeSlider.value = [mgr.p44BTDMX getChannel:lightBase+4];
  self.speedSlider.value = [mgr.p44BTDMX getChannel:lightBase+5];
  self.gradientSlider.value = [mgr.p44BTDMX getChannel:lightBase+6];
  self.modeSelect.selectedSegmentIndex = [mgr.p44BTDMX getChannel:lightBase+7] & 0x3F;
  self.modeHiSelect.selectedSegmentIndex = ([mgr.p44BTDMX getChannel:lightBase+7]>>6) & 0x03;
}


- (void)showLightInfo
{
  [self.lightNoLabel setText:[NSString stringWithFormat:@"Light #%02d (DMX %03d..%03d)", mCurrentLightNo, mCurrentLightNo*LIGHT_CHANNELS+1, mCurrentLightNo*LIGHT_CHANNELS+LIGHT_CHANNELS]];
}


- (void)restartBroadcast
{
  if (mLocallyStopped) {
    mLocallyStopped = NO;
    [[AppDelegate sharedAppDelegate].p44BTDMXManager startBroadcast];
  }
}


- (IBAction)lightNo1sChanged:(id)sender
{
  [self updateLightNoFromControls];
  [self updateChannels];
}

- (IBAction)lightNo10sChanged:(id)sender
{
  [self updateLightNoFromControls];
  [self updateChannels];
}


- (void)changeChannel:(int)channel toValue:(int)value
{
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+channel toValue:value];
  [self restartBroadcast];
  [self.lightNoLabel setText:[NSString stringWithFormat:@"Channel %01d (DMX %03d) : %03d", channel, mCurrentLightNo*LIGHT_CHANNELS+channel+1, value]];
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [self performSelector:@selector(channelChangedTimeout) withObject:self afterDelay:2];
}

- (void)channelChangedTimeout
{
  [self showLightInfo];
}



- (IBAction)hueChanged:(id)sender
{
  [self changeChannel:0 toValue:self.hueSlider.value];
}

- (IBAction)saturationChanged:(id)sender
{
  [self changeChannel:1 toValue:self.saturationSlider.value];
}

- (IBAction)brightnessChanged:(id)sender
{
  [self changeChannel:2 toValue:self.brightnessSlider.value];
}


- (IBAction)positionChanged:(id)sender
{
  [self changeChannel:3 toValue:self.positionSlider.value];
}

- (IBAction)sizeChanged:(id)sender
{
  [self changeChannel:4 toValue:self.sizeSlider.value];
}

- (IBAction)speedChanged:(id)sender
{
  [self changeChannel:5 toValue:self.speedSlider.value];
}

- (IBAction)gradientChanged:(id)sender
{
  [self changeChannel:6 toValue:self.gradientSlider.value];
}


- (IBAction)modeChanged:(id)sender
{
  [self changeChannel:7 toValue:self.modeSelect.selectedSegmentIndex + (self.modeHiSelect.selectedSegmentIndex << 6)];
}


- (IBAction)resetUniverse:(id)sender
{
  // reset brightness: all off, but detail settings undisturbed
  for (int light=0; light<NUM_LIGHTS; light++) {
    [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:light*LIGHT_CHANNELS+2 toValue:0]; // reset brightness
  }
  [self updateChannels];
  [self restartBroadcast];
}


- (IBAction)clearUniverse:(id)sender
{
  for (int light=0; light<NUM_LIGHTS; light++) {
    for (int channel=0; channel<LIGHT_CHANNELS; channel++) {
      [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:light*LIGHT_CHANNELS+channel toValue:0];
    }
  }
  [self updateChannels];
  [self restartBroadcast];
}



- (IBAction)stopBroadcastTapped:(id)sender
{
  mLocallyStopped = YES;
  [[AppDelegate sharedAppDelegate].p44BTDMXManager stopBroadcast];
}


- (IBAction)endSettings:(UIStoryboardSegue*)unwindSegue
{
  SettingsViewController *settings = (SettingsViewController *)[unwindSegue sourceViewController];
  [[NSUserDefaults standardUserDefaults] setValue:settings.systemKeyTextfield.text forKey:@"p44BtDMXsystemKey"];
  [[NSUserDefaults standardUserDefaults] setBool:settings.refreshUniverseSwitch.on forKey:@"p44BtDMXrefreshUniverse"];
  [[AppDelegate sharedAppDelegate] readConfig];
  [self updateChannels];
}



@end
