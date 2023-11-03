# RewardsTheater

An [OBS](https://obsproject.com/) plugin that lets your viewers redeem videos or sounds on stream via Twitch Channel Points.

[Watch this video](https://youtu.be/-0evZCAlXVU) on how to set everything up.
Or you can read the full instructions below.

[Прочитати українською тут](README_uk.md)

## How's this better than [TR!GGER FYRE](https://overlays.thefyrewire.com/widgets/triggerfyre/)?

- Free and open source.
- Use your locally saved videos, no need to upload anything to the server.
- Has a nice UI to edit channel point rewards right inside OBS!
- Plays the videos natively in OBS without a browser source. This means better FPS and quality.
- You can position videos conveniently inside OBS itself.
- Reward redemptions can be put in a queue so that they don't play simultaneously.

![ui](readme_images/ui.png)

## Requirements
- You are a Twitch Affiliate or Partner and have channel points enabled.
- OBS version 29.1.2 or newer.
- Windows or Linux operating system.


## Setting up
1. Download and run the installer for your operating system from [this link](https://github.com/gottagofaster236/RewardsTheater/releases/latest).
2. RewardsTheater supports videos and sounds. The instructions are identical in both cases.
3. - Add an OBS source for each video you want to play as a channel points reward. It has to be either Media Source or VLC Video Source:
     
     ![source](readme_images/source.png)
   - Select a video file for the source from your computer. If you want the video to appear and hide smoothly, uncheck "Show nothing when playback ends" and select show/hide transitions for the source:
     
     ![media_source](readme_images/media_source.png)
     ![transition](readme_images/transition.png)
   - Position the source on the scene in the way that you like.
   - Then hide the source, so that it doesn't play every time you start OBS:
     
     ![hide](readme_images/hide.png)
4. Then go to Tools → RewardsTheater.
   
   ![tools](readme_images/tools.png)
5. You'll have to log into Twitch first by clicking the "Log in" button.
6. After that, you can hit "Add reward". Select the source you added earlier as the Media Source for the reward. 
**Hit "Test source" in order to test whether the source will work.**
 Due to the limitations of Twitch API, if you want to upload a custom icon, you're gonna have to do this inside the browser.

   
   ![add_reward](readme_images/add_reward.png)

7. You can edit the reward later by clicking on it in the Tools → RewardsTheater menu.

   ![settings](readme_images/settings.png)

8. If there are a lot of rewards, you can put the media sources onto a separate scene, and add that scene to other scenes.

9. During stream, you can monitor the reward queue if there's a lot of redemptions at the same time and cancel them by clicking the cross button. The viewer's channel points are refunded if you cancel a reward.

   ![rewards_queue](readme_images/rewards_queue.png)

10. You can click "Pause reward playback" on the main settings screen if you don't want the videos playing for some time. During this time, the user will get their points refunded as well.

## Supporting the developer
If you liked my work, please leave a star on the GitHub repository 🙂

## Building
PRs are welcome! If you want to build RewardsTheater yourself, please refer to [BUILDING.md](BUILDING.md)

## License and credits
- RewardsTheater is licensed under GNU General Public License v3.0. 
- RewardsTheater is a plugin to [OBS Studio](https://github.com/obsproject/obs-studio), which is licensed under GNU General Public License v2.0 or later.
- RewardsTheater uses [Boost.Asio](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio.html), [Boost.URL](https://www.boost.org/doc/libs/1_83_0/libs/url/doc/html/index.html), [Boost.Beast](https://www.boost.org/doc/libs/1_83_0/libs/beast/doc/html/index.html), [Boost.JSON](https://www.boost.org/doc/libs/1_83_0/libs/json/doc/html/index.html), which are licensed under the Boost Software License, Version 1.0.
- RewardsTheater uses [Qt Core](https://doc.qt.io/qt-6/qtcore-index.html), [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html) and [Qt GUI](https://doc.qt.io/qt-6/qtgui-index.html) modules, which are available under GNU General Public License v2.0 or later.
- RewardsTheater uses [OpenSSL](https://openssl.org/), which is licensed under Apache-2.0 License.
- RewardTheater uses Google's [material-design-icons](https://github.com/google/material-design-icons/tree/master), which are licensed under Apache-2.0 license.
