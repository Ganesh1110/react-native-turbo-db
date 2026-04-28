# rn_turbo_db_post_install.rb
#
# Usage — add ONE line to your Podfile's post_install block:
#
#   post_install do |installer|
#     react_native_post_install(installer, config[:reactNativePath], :mac_catalyst_enabled => false)
#     RNTurboDBPostInstall.apply(installer)   # ← add this
#   end
#
# What it does:
#   Stamps -DFOLLY_HAS_COROUTINES=0 onto every pod target so that
#   folly/Expected.h and folly/Optional.h don't try to include the missing
#   folly/coro/ directory that ReactNativeDependencies doesn't ship.

module RNTurboDBPostInstall
  def self.apply(installer)
    installer.pods_project.targets.each do |target|
      target.build_configurations.each do |config|
        flags = config.build_settings['OTHER_CPLUSPLUSFLAGS'] || '$(inherited)'
        next if flags.to_s.include?('FOLLY_HAS_COROUTINES')
        config.build_settings['OTHER_CPLUSPLUSFLAGS'] =
          "#{flags} -DFOLLY_HAS_COROUTINES=0"
      end
    end
  end
end
