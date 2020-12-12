//
//  ViewController.h
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import <UIKit/UIKit.h>

@interface ViewController : UIViewController

@property (weak, nonatomic) IBOutlet UISegmentedControl *lightNo10s;
@property (weak, nonatomic) IBOutlet UISegmentedControl *lightNo1s;

@property (weak, nonatomic) IBOutlet UISlider *hueSlider;
@property (weak, nonatomic) IBOutlet UISlider *saturationSlider;
@property (weak, nonatomic) IBOutlet UISlider *brightnessSlider;

@property (weak, nonatomic) IBOutlet UISlider *positionSlider;

@property (weak, nonatomic) IBOutlet UISegmentedControl *modeSelect;

- (IBAction)lightNo10sChanged:(id)sender;
- (IBAction)lightNo1sChanged:(id)sender;

- (IBAction)hueChanged:(id)sender;
- (IBAction)saturationChanged:(id)sender;
- (IBAction)brightnessChanged:(id)sender;

- (IBAction)positionChanged:(id)sender;

- (IBAction)modeChanged:(id)sender;

@end

