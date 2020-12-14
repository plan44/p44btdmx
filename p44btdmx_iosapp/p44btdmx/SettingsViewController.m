//
//  SettingsViewController.m
//  p44btdmx
//
//  Created by Lukas Zeller on 14.12.20.
//

#import "SettingsViewController.h"
#import "AppDelegate.h"

@interface SettingsViewController ()

@end

@implementation SettingsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}


- (void)viewWillAppear:(BOOL)animated
{
  self.systemKeyTextfield.text = [[NSUserDefaults standardUserDefaults] objectForKey:@"p44BtDMXsystemKey"];
  [super viewWillAppear:animated];
}



/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
