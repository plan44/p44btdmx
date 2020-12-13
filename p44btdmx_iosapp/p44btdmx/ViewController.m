//
//  ViewController.m
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import "ViewController.h"
#import "p44btdmx.h"
#import "AppDelegate.h"

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
  int lightBase = mCurrentLightNo*5;
  self.hueSlider.value = [mgr.p44BTDMX getChannel:lightBase+0];
  self.saturationSlider.value = [mgr.p44BTDMX getChannel:lightBase+1];
  self.brightnessSlider.value = [mgr.p44BTDMX getChannel:lightBase+2];
  self.positionSlider.value = [mgr.p44BTDMX getChannel:lightBase+3];
  self.modeSelect.selectedSegmentIndex = ([mgr.p44BTDMX getChannel:lightBase+4]>>4) & 0xF;
  self.modeParamSlider.value = [mgr.p44BTDMX getChannel:lightBase+4] & 0xF;
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
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*5+0 toValue:self.hueSlider.value];
}

- (IBAction)saturationChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*5+1 toValue:self.saturationSlider.value];
}

- (IBAction)brightnessChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*5+2 toValue:self.brightnessSlider.value];
}


- (IBAction)positionChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*5+3 toValue:self.positionSlider.value];
}


- (IBAction)modeChanged:(id)sender
{
  [self restartBroadcast];
  [[AppDelegate sharedAppDelegate].p44BTDMXManager.p44BTDMX setChannel:mCurrentLightNo*5+4 toValue:(self.modeSelect.selectedSegmentIndex<<4)+((int)self.modeParamSlider.value&0xF)];
}



- (IBAction)stopBroadcastTapped:(id)sender
{
  mLocallyStopped = YES;
  [[AppDelegate sharedAppDelegate].p44BTDMXManager stopBroadcast];
}




@end
