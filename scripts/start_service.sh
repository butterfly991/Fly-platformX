#!/bin/bash

# Cloud IaaS Service Startup Script
# Version: 1.0.0

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_NAME="CloudIaaS"
BUILD_DIR="build"
BIN_DIR="${BUILD_DIR}/bin"
CONFIG_DIR="config"
LOGS_DIR="logs"
RECOVERY_DIR="recovery_points"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check system requirements
check_system_requirements() {
    print_status "Checking system requirements..."
    
    # Check OS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        print_success "Detected macOS"
        PLATFORM="apple_arm"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        print_success "Detected Linux"
        PLATFORM="linux_x64"
    else
        print_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi
    
    # Check required commands
    local required_commands=("cmake" "make" "g++" "clang++")
    for cmd in "${required_commands[@]}"; do
        if command_exists "$cmd"; then
            print_success "Found $cmd"
        else
            print_warning "Command $cmd not found"
        fi
    done
    
    # Check required libraries
    if command_exists "brew"; then
        print_success "Found Homebrew package manager"
    elif command_exists "apt"; then
        print_success "Found APT package manager"
    else
        print_warning "No supported package manager found"
    fi
}

# Function to create necessary directories
create_directories() {
    print_status "Creating necessary directories..."
    
    mkdir -p "$BUILD_DIR"
    mkdir -p "$LOGS_DIR"
    mkdir -p "$RECOVERY_DIR"
    mkdir -p "$CONFIG_DIR"
    
    print_success "Directories created"
}

# Function to build the project
build_project() {
    print_status "Building project..."
    
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCLOUD_ENABLE_LOGGING=ON \
        -DCLOUD_ENABLE_METRICS=ON \
        -DCLOUD_ENABLE_PERFORMANCE_OPTIMIZATIONS=ON \
        -DBUILD_TESTING=ON
    
    # Build
    print_status "Building..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cd ..
    
    if [[ -f "$BIN_DIR/$SERVICE_NAME" ]]; then
        print_success "Build completed successfully"
    else
        print_error "Build failed - executable not found"
        exit 1
    fi
}

# Function to run tests
run_tests() {
    print_status "Running tests..."
    
    cd "$BUILD_DIR"
    
    # Run all tests
    if make test; then
        print_success "All tests passed"
    else
        print_warning "Some tests failed, but continuing..."
    fi
    
    cd ..
}

# Function to start the service
start_service() {
    print_status "Starting Cloud IaaS Service..."
    
    # Check if service is already running
    if pgrep -f "$SERVICE_NAME" > /dev/null; then
        print_warning "Service is already running"
        read -p "Do you want to stop the existing service and restart? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            stop_service
        else
            print_status "Exiting..."
            exit 0
        fi
    fi
    
    # Set environment variables
    export CLOUD_SERVICE_CONFIG="$CONFIG_DIR/cloud_service.json"
    export CLOUD_LOG_LEVEL="debug"
    
    # Start the service
    print_status "Launching service..."
    "$BIN_DIR/$SERVICE_NAME" &
    SERVICE_PID=$!
    
    # Wait a moment and check if service started
    sleep 2
    if kill -0 "$SERVICE_PID" 2>/dev/null; then
        print_success "Service started successfully (PID: $SERVICE_PID)"
        echo "$SERVICE_PID" > "$LOGS_DIR/service.pid"
        
        # Show logs
        print_status "Service logs (Ctrl+C to stop):"
        tail -f "$LOGS_DIR/cloud_service.log"
    else
        print_error "Service failed to start"
        exit 1
    fi
}

# Function to stop the service
stop_service() {
    print_status "Stopping Cloud IaaS Service..."
    
    # Try to find PID from file
    if [[ -f "$LOGS_DIR/service.pid" ]]; then
        local pid=$(cat "$LOGS_DIR/service.pid")
        if kill -0 "$pid" 2>/dev/null; then
            print_status "Sending SIGTERM to process $pid"
            kill -TERM "$pid"
            
            # Wait for graceful shutdown
            local count=0
            while kill -0 "$pid" 2>/dev/null && [[ $count -lt 30 ]]; do
                sleep 1
                ((count++))
            done
            
            # Force kill if still running
            if kill -0 "$pid" 2>/dev/null; then
                print_warning "Force killing process $pid"
                kill -KILL "$pid"
            fi
            
            rm -f "$LOGS_DIR/service.pid"
            print_success "Service stopped"
        else
            print_warning "Service not running (PID: $pid)"
            rm -f "$LOGS_DIR/service.pid"
        fi
    else
        # Try to find by name
        local pids=$(pgrep -f "$SERVICE_NAME" 2>/dev/null || true)
        if [[ -n "$pids" ]]; then
            print_status "Found running processes: $pids"
            echo "$pids" | xargs kill -TERM
            sleep 2
            echo "$pids" | xargs kill -KILL 2>/dev/null || true
            print_success "Service stopped"
        else
            print_warning "No running service found"
        fi
    fi
}

# Function to show service status
show_status() {
    print_status "Service status:"
    
    if [[ -f "$LOGS_DIR/service.pid" ]]; then
        local pid=$(cat "$LOGS_DIR/service.pid")
        if kill -0 "$pid" 2>/dev/null; then
            print_success "Service is running (PID: $pid)"
            
            # Show basic info
            echo "  - Uptime: $(ps -o etime= -p "$pid" 2>/dev/null || echo "Unknown")"
            echo "  - Memory: $(ps -o rss= -p "$pid" 2>/dev/null | awk '{print $1/1024 " MB"}' || echo "Unknown")"
            echo "  - CPU: $(ps -o %cpu= -p "$pid" 2>/dev/null || echo "Unknown")%"
        else
            print_warning "Service PID file exists but process not running"
            rm -f "$LOGS_DIR/service.pid"
        fi
    else
        print_warning "Service not running"
    fi
    
    # Show log file info
    if [[ -f "$LOGS_DIR/cloud_service.log" ]]; then
        echo "  - Log file: $LOGS_DIR/cloud_service.log ($(du -h "$LOGS_DIR/cloud_service.log" | cut -f1))"
    fi
}

# Function to show help
show_help() {
    echo "Cloud IaaS Service Management Script"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  start     - Build and start the service"
    echo "  stop      - Stop the service"
    echo "  restart   - Restart the service"
    echo "  status    - Show service status"
    echo "  build     - Build the project only"
    echo "  test      - Run tests only"
    echo "  clean     - Clean build directory"
    echo "  help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 start    # Build and start service"
    echo "  $0 status   # Check service status"
    echo "  $0 stop     # Stop service"
}

# Function to clean build
clean_build() {
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    print_success "Build directory cleaned"
}

# Main script logic
main() {
    case "${1:-start}" in
        start)
            check_system_requirements
            create_directories
            build_project
            run_tests
            start_service
            ;;
        stop)
            stop_service
            ;;
        restart)
            stop_service
            sleep 2
            check_system_requirements
            create_directories
            build_project
            start_service
            ;;
        status)
            show_status
            ;;
        build)
            check_system_requirements
            create_directories
            build_project
            ;;
        test)
            check_system_requirements
            create_directories
            build_project
            run_tests
            ;;
        clean)
            clean_build
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@" 