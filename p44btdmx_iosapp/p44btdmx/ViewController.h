//
//  ViewController.h
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import <UIKit/UIKit.h>

@interface ViewController : UIViewController

@property (weak, nonatomic) IBOutlet UILabel *lightNoLabel;
@property (weak, nonatomic) IBOutlet UISegmentedControl *lightNo10s;
@property (weak, nonatomic) IBOutlet UISegmentedControl *lightNo1s;

@property (weak, nonatomic) IBOutlet UISlider *hueSlider;
@property (weak, nonatomic) IBOutlet UISlider *saturationSlider;
@property (weak, nonatomic) IBOutlet UISlider *brightnessSlider;

@property (weak, nonatomic) IBOutlet UISlider *positionSlider;
@property (weak, nonatomic) IBOutlet UISlider *sizeSlider;

@property (weak, nonatomic) IBOutlet UISegmentedControl *modeSelect;
@property (weak, nonatomic) IBOutlet UISegmentedControl *modeHiSelect;
@property (weak, nonatomic) IBOutlet UISlider *speedSlider;
@property (weak, nonatomic) IBOutlet UISlider *gradientSlider;

- (IBAction)lightNo10sChanged:(id)sender;
- (IBAction)lightNo1sChanged:(id)sender;

- (IBAction)hueChanged:(id)sender;
- (IBAction)saturationChanged:(id)sender;
- (IBAction)brightnessChanged:(id)sender;

- (IBAction)positionChanged:(id)sender;
- (IBAction)sizeChanged:(id)sender;

- (IBAction)modeChanged:(id)sender;
- (IBAction)speedChanged:(id)sender;
- (IBAction)gradientChanged:(id)sender;

- (IBAction)stopBroadcastTapped:(id)sender;
- (IBAction)resetUniverse:(id)sender;
- (IBAction)clearUniverse:(id)sender;

- (IBAction)endSettings:(UIStoryboardSegue*)unwindSegue;

@end

