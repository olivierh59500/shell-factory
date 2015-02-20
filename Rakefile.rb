require 'rake'
require 'rake/clean'

require 'ipaddr'

class IPAddr
    def to_define
        if self.ipv4?
            "\\{#{self.to_string.gsub(".",",")}\\}"
        else
            "\\{#{
            self.to_string.gsub(":","")
                .split(/(..)/)
                .delete_if{|x| x.empty?}
                .map{|d| "0x" + d}
                .join(',')
            }\\}"
        end
    end
end

CC = "g++"
OUTPUT_DIR = "bins"
INCLUDE_DIRS = %w{include}
CFLAGS = %w{-std=c++1y
            -Wall
            -Wextra
            -Wfatal-errors
            -fno-common
            -fomit-frame-pointer
            -nostdlib
            -Wl,-e_start
            -Wl,--gc-sections
         }

COMPILER_CFLAGS =
{
    /^g\+\+$/ => %w{-fno-toplevel-reorder
                  -finline-functions
                  -nodefaultlibs
                  -Os
               },

    /^clang\+\+$/ => %w{-Oz}
}

# Architecture-dependent flags.
ARCH_CFLAGS =
{
    /mips/ => %w{-mshared -mno-abicalls -mno-plt -mno-gpopt -mno-long-calls -G 0},
    /.*/ => %w{-fPIC}
}

def cc_invoke(cc, triple)
    return cc if triple.empty?

    case cc
    when 'g++'
        "#{triple}-#{cc}"

    when 'clang++'
        "#{cc} -target #{triple} --sysroot /usr/#{triple}/"
    end
end

def compile(target, triple, output_dir, *opts)
    common_opts = %w{CHANNEL HOST PORT NO_BUILTIN FORK_ON_ACCEPT REUSE_ADDR}
    options = common_opts + opts
    defines = ENV.select{|e| options.include?(e)}
    options = common_opts + opts
    cc = ENV['CC'] || CC
    cflags = CFLAGS.dup

    puts "[*] Generating shellcode '#{target}'"
    puts "    |\\ Compiler: #{cc}"
    puts "    |\\ Target architecture: #{triple.empty? ? `uname -m` : triple}"
    puts "     \\ Options: #{defines}"
    puts

    ARCH_CFLAGS.each_pair { |arch, flags|
        if triple =~ arch
            cflags += flags
            break
        end
    }

    COMPILER_CFLAGS.each_pair { |comp, flags|
        if cc =~ comp
            cflags += flags
            break
        end
    }

    if ENV['CFLAGS']
        cflags += [ ENV['CFLAGS'] ]
    end
    
    if defines['NO_BUILTIN'] and defines['NO_BUILTIN'].to_i == 1
        cflags << "-fno-builtin"
    end

    if ENV['OUTPUT_LIB'] and ENV['OUTPUT_LIB'].to_i == 1
        cflags << '-shared'
    end

    cflags += INCLUDE_DIRS.map{|d| "-I#{d}"}
    defines = defines.map{|k,v|
        v = IPAddr.new(v).to_define if k == 'HOST'
        "-D#{k}=#{v}"
    }

    if ENV['OUTPUT_DEBUG'] and ENV['OUTPUT_DEBUG'].to_i == 1
        sh "#{cc_invoke(cc,triple)} -S #{cflags.join(" ")} shellcodes/#{target}.cc -o #{output_dir}/#{target}.S #{defines.join(' ')}"
    end

    sh "#{cc_invoke(cc,triple)} #{cflags.join(' ')} shellcodes/#{target}.cc -o #{output_dir}/#{target}.elf #{defines.join(' ')}"
end

def generate_shellcode(target, triple, output_dir)
    triple += '-' unless triple.empty?
    sh "#{triple}objcopy -O binary -j .text -j .funcs -j .rodata bins/#{target}.elf #{output_dir}/#{target}.bin" 

    puts
    puts "[*] Generated shellcode: #{File.size("#{output_dir}/#{target}.bin")} bytes."
end

def build(target, *opts)
    output_dir = OUTPUT_DIR
    triple = ''
    triple = ENV['TRIPLE'] if ENV['TRIPLE']

    compile(target, triple, output_dir, *opts)
    generate_shellcode(target, triple, output_dir)
end

task :shellexec do
    build(:shellexec, "COMMAND", "SET_ARGV0")
end

rule '' do |task|
    build(task.name)
end

CLEAN.include("bins/*.{elf,bin}")
