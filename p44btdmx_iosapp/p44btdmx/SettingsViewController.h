//
//  SettingsViewController.h
//  p44btdmx
//
//  Created by Lukas Zeller on 14.12.20.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SettingsViewController : UIViewController

@property (weak, nonatomic) IBOutlet UITextField *systemKeyTextfield;
@property (weak, nonatomic) IBOutlet UISwitch *refreshUniverseSwitch;

@end

NS_ASSUME_NONNULL_END
