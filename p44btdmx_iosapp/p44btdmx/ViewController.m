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
}
@end


@implementation ViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  // Do any additional setup after loading the view.
  mCurrentLightNo = 0;
  mLocallyStopped = NO;
}


- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [self updateChannels];
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
}


- (void)updateChannels
{
  P44BTDMXManager *mgr = [AppDelegate sharedAppDelegate].p44BTDMXManager;
  [self updateLightNoFromControls];
  [self.lightNoLabel setText:[NSString stringWithFormat:@"Light #%d = DMX %d..%d", mCurrentLightNo, mCurrentLightNo*LIGHT_CHANNELS+1, mCurrentLightNo*LIGHT_CHANNELS+LIGHT_CHANNELS]];
  int lightBase = mCurrentLightNo*LIGHT_CHANNELS;
  self.hueSlider.value = [mgr.p44BTDMX getChannel:lightBase+0];
  self.saturationSlider.value = [mgr.p44BTDMX getChannel:lightBase+1];
  self.brightnessSlider.value = [mgr.p44BTDMX getChannel:lightBase+2];
  self.positionSlider.value = [mgr.p44BTDMX getChannel:lightBase+3];
  self.sizeSlider.value = [mgr.p44BTDMX getChannel:lightBase+4];
  self.speedSlider.value = [mgr.p44BTDMX getChannel:lightBase+5];
  self.gradientSlider.value = [mgr.p44BTDMX getChannel:lightBase+6];
  self.modeSelect.selectedSegmentIndex = [mgr.p44BTDMX getChannel:lightBase+7];
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
  [self updateChannels];
}

- (IBAction)lightNo10sChanged:(id)sender
{
  [self updateChannels];
}


- (IBAction)hueChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+0 toValue:self.hueSlider.value];
}

- (IBAction)saturationChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+1 toValue:self.saturationSlider.value];
}

- (IBAction)brightnessChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+2 toValue:self.brightnessSlider.value];
}


- (IBAction)positionChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+3 toValue:self.positionSlider.value];
}

- (IBAction)sizeChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+4 toValue:self.sizeSlider.value];
}

- (IBAction)speedChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+5 toValue:self.speedSlider.value];
}

- (IBAction)gradientChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+6 toValue:self.gradientSlider.value];
}


- (IBAction)modeChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*LIGHT_CHANNELS+7 toValue:self.modeSelect.selectedSegmentIndex];
}


- (IBAction)resetUniverse:(id)sender
{
  // reset brightness and mode only: all off, but detail settings undisturbed
  for (int light=0; light<NUM_LIGHTS; light++) {
    [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:light*LIGHT_CHANNELS+2 toValue:0]; // reset brightness
    [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:light*LIGHT_CHANNELS+7 toValue:0]; // reset mode
  }
  [self updateChannels];
}


- (IBAction)clearUniverse:(id)sender
{
  for (int light=0; light<NUM_LIGHTS; light++) {
    for (int channel=0; channel<LIGHT_CHANNELS; channel++) {
      [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:light*LIGHT_CHANNELS+channel toValue:0];
    }
  }
  [self updateChannels];
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
