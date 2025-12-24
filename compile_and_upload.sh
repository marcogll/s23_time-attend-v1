#!/bin/bash
# Script para compilar, cargar y revisar el monitor serial del checador NFC

# Colores para output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Configuración
# Obtener el directorio del script para hacerlo portable
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="$SCRIPT_DIR"
SKETCH_NAME="soul23_time_attendance"
FQBN="esp8266:esp8266:nodemcuv2"  # NodeMCU 1.0 (ESP-12E Module)
PORT="/dev/ttyUSB0"
BAUD_RATE="115200"

# Función para mostrar barra de progreso
show_progress() {
    local current=$1
    local total=$2
    local width=50
    local percent=$((current * 100 / total))
    local filled=$((current * width / total))
    local empty=$((width - filled))
    
    printf "\r${CYAN}["
    printf "%${filled}s" | tr ' ' '█'
    printf "%${empty}s" | tr ' ' '░'
    printf "] ${percent}%%${NC}"
}

# Función para verificar estado del dispositivo
check_device_status() {
    local port=$1
    local status=""
    local details=""
    
    if [ -e "$port" ]; then
        # Verificar si el puerto está en la lista de arduino-cli
        local board_list=$(arduino-cli board list 2>/dev/null)
        if echo "$board_list" | grep -q "$port"; then
            local board_info=$(echo "$board_list" | grep "$port")
            local protocol=$(echo "$board_info" | awk '{print $2}')
            local board_type=$(echo "$board_info" | awk '{print $3}')
            
            if echo "$board_info" | grep -q "Unknown"; then
                status="${YELLOW}⚠️  Conectado${NC}"
                details="(Placa no identificada - Protocolo: $protocol)"
            else
                local board_name=$(echo "$board_info" | awk '{for(i=4;i<=NF;i++) printf $i" "; print ""}')
                status="${GREEN}✅ Conectado${NC}"
                details="($board_name)"
            fi
        else
            status="${YELLOW}⚠️  Puerto existe${NC}"
            details="(No detectado por arduino-cli - puede requerir permisos)"
        fi
    else
        status="${RED}❌ No conectado${NC}"
        details="(Puerto $port no encontrado)"
    fi
    
    echo -e "$status $details"
}

# Función para detectar puerto automáticamente
detect_port() {
    local detected_port=$(arduino-cli board list | grep -E "ttyUSB|ttyACM" | head -1 | awk '{print $1}')
    if [ -n "$detected_port" ]; then
        echo "$detected_port"
    else
        echo ""
    fi
}

# Encabezado
clear
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║     Checador NFC - Compilación y Carga Automática      ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════╝${NC}\n"

# Paso 0: Verificar secrets.h
echo -e "${BOLD}${CYAN}[0/4]${NC} Verificando configuración..."
if [ ! -f "$SKETCH_DIR/secrets.h" ]; then
    echo -e "${RED}❌ Error: No se encuentra secrets.h${NC}"
    echo -e "${YELLOW}💡 Crea secrets.h desde secrets.h.example y configura tus credenciales${NC}"
    exit 1
fi
echo -e "${GREEN}✅ secrets.h encontrado${NC}\n"

# Paso 1: Verificar y detectar dispositivo
echo -e "${BOLD}${CYAN}[1/4]${NC} Verificando conexión del dispositivo..."
echo -e "   Puerto configurado: ${BOLD}$PORT${NC}"

# Verificar puerto configurado
echo -e "   Estado: $(check_device_status "$PORT")"

if [ ! -e "$PORT" ] || ! arduino-cli board list 2>/dev/null | grep -q "$PORT"; then
    echo -e "\n${YELLOW}🔍 Buscando dispositivos disponibles...${NC}"
    
    # Intentar detectar automáticamente
    auto_port=$(detect_port)
    if [ -n "$auto_port" ]; then
        echo -e "${GREEN}   ✓ Dispositivo detectado en: $auto_port${NC}"
        read -p "   ¿Usar este puerto? (s/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[SsYy]$ ]]; then
            PORT="$auto_port"
            echo -e "   Estado: $(check_device_status "$PORT")"
        else
            echo -e "${RED}❌ Por favor, conecta el dispositivo y vuelve a intentar${NC}"
            exit 1
        fi
    else
        echo -e "${RED}   ✗ No se encontraron dispositivos${NC}"
        echo -e "\n${YELLOW}Puertos disponibles:${NC}"
        arduino-cli board list
        echo -e "\n${YELLOW}Por favor, conecta el dispositivo o modifica PORT en el script${NC}"
        exit 1
    fi
fi

echo -e "   Puerto seleccionado: ${BOLD}$PORT${NC}\n"

# Paso 2: Compilar
echo -e "${BOLD}${CYAN}[2/4]${NC} Compilando el sketch..."
echo -e "   FQBN: ${BOLD}$FQBN${NC}"
echo -e "   Sketch: ${BOLD}$SKETCH_NAME.ino${NC}\n"

# Compilar con barra de progreso animada
compile_output=$(mktemp)
(
    arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR/$SKETCH_NAME.ino" > "$compile_output" 2>&1
    echo $? > "${compile_output}.exit"
) &
compile_pid=$!

# Mostrar barra de progreso animada mientras compila
spinner="|/-\\"
spinner_idx=0
while kill -0 $compile_pid 2>/dev/null; do
    spinner_char=${spinner:$spinner_idx:1}
    printf "\r   ${CYAN}[${spinner_char}]${NC} Compilando... ${CYAN}%s${NC}" "$spinner_char"
    spinner_idx=$(( (spinner_idx + 1) % 4 ))
    sleep 0.2
done

wait $compile_pid
compile_result=$(cat "${compile_output}.exit" 2>/dev/null || echo "1")
rm -f "${compile_output}.exit"

printf "\r   ${GREEN}[✓]${NC} Compilación completada                    \n"

# Mostrar errores si los hay
if [ $compile_result -ne 0 ]; then
    echo -e "\n${RED}❌ Error en la compilación:${NC}"
    cat "$compile_output" | grep -i "error\|warning" | head -10
    rm -f "$compile_output"
    exit 1
fi

# Mostrar resumen de compilación
if [ -s "$compile_output" ]; then
    size_info=$(grep -i "sketch uses\|global variables" "$compile_output" | head -2)
    if [ -n "$size_info" ]; then
        echo -e "   ${BLUE}ℹ️${NC} $(echo "$size_info" | tr '\n' ' ')"
    fi
fi
rm -f "$compile_output"

echo -e "${GREEN}✅ Compilación exitosa${NC}\n"

# Paso 3: Cargar al dispositivo
echo -e "${BOLD}${CYAN}[3/4]${NC} Cargando firmware al dispositivo..."
echo -e "   Puerto: ${BOLD}$PORT${NC}"

# Verificar nuevamente que el dispositivo está conectado
if [ ! -e "$PORT" ]; then
    echo -e "${RED}❌ Error: El dispositivo se desconectó${NC}"
    exit 1
fi

# Verificar estado antes de cargar
echo -e "   Estado: $(check_device_status "$PORT")\n"

# Cargar con indicador de progreso
upload_output=$(mktemp)
(
    arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR/$SKETCH_NAME.ino" > "$upload_output" 2>&1
    echo $? > "${upload_output}.exit"
) &
upload_pid=$!

# Mostrar spinner animado durante la carga
spinner="|/-\\"
spinner_idx=0
stage=0
stages=("Iniciando" "Escribiendo" "Verificando" "Completando")
while kill -0 $upload_pid 2>/dev/null; do
    spinner_char=${spinner:$spinner_idx:1}
    # Cambiar etapa cada 2 segundos aproximadamente
    if [ $((spinner_idx % 10)) -eq 0 ] && [ $stage -lt ${#stages[@]} ]; then
        stage=$((stage + 1))
    fi
    if [ $stage -ge ${#stages[@]} ]; then
        stage=$((stage % ${#stages[@]}))
    fi
    printf "\r   ${CYAN}[${spinner_char}]${NC} ${stages[$stage]}... ${CYAN}%s${NC}" "$spinner_char"
    spinner_idx=$(( (spinner_idx + 1) % 4 ))
    sleep 0.2
done

wait $upload_pid
upload_result=$(cat "${upload_output}.exit" 2>/dev/null || echo "1")
rm -f "${upload_output}.exit"

printf "\r   ${GREEN}[✓]${NC} Carga completada                          \n"

if [ $upload_result -ne 0 ]; then
    echo -e "\n${RED}❌ Error al cargar el firmware:${NC}"
    cat "$upload_output" | grep -i "error\|failed\|timeout" | head -5
    rm -f "$upload_output"
    echo -e "\n${YELLOW}💡 Verifica que:${NC}"
    echo -e "   • El dispositivo esté conectado"
    echo -e "   • No haya otro programa usando el puerto"
    echo -e "   • Presiona el botón RESET del ESP8266 si es necesario"
    echo -e "   • Mantén presionado BOOT mientras cargas si es necesario"
    exit 1
fi

rm -f "$upload_output"
echo -e "${GREEN}✅ Firmware cargado exitosamente${NC}\n"

# Paso 4: Monitor serial
echo -e "${BOLD}${CYAN}[4/4]${NC} Abriendo monitor serial..."
echo -e "   Puerto: ${BOLD}$PORT${NC}"
echo -e "   Baud rate: ${BOLD}$BAUD_RATE${NC}"
echo -e "\n${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}   Monitor Serial (Presiona ${BOLD}Ctrl+C${NC}${YELLOW} para salir)${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"

# Pequeña pausa antes de abrir el monitor
sleep 1

# Abrir monitor serial
arduino-cli monitor -p "$PORT" --config baudrate="$BAUD_RATE"
