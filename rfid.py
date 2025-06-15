# Importando as bibliotecas
import tkinter as tk # tkinter interface gráfica
from tkinter import messagebox, ttk
import serial # Comunicação com a Esp 32 pela porta COM
import time
import threading # Evita travamento da interface enquanto espera a leitura
import csv  # Grava e lê histórico das leituras em arquivo csv
from datetime import datetime # Adiciona data e hora
from openpyxl import Workbook # Exporta o histórico para uma planilha excel

PORTA_SERIAL = 'COM7' # Porta usada para comunicação com a Esp 32
BAUDRATE = 9600 # Velocidade da conexão com a serial

# A classe RFIDApp cria a interface e tambpem controla as funcionalidades
class RFIDApp:
    def __init__(self, root): # O __init__ constrói a interface
        # Cria a janela principal, colocando o título e o tamannho
        self.root = root
        self.root.title("Controle RFID - ESP32")
        self.root.geometry("450x660")

        # Criação dos componentes da interface, como labels, botões, subtitulos
        self.serial = None
        self.leitura_em_andamento = False
        self.conectar_serial()

        tk.Label(root, text="Leitor de Cartão RFID", font=("Arial", 16)).pack(pady=10)

        tk.Label(root, text="UID do Cartão:", font=("Arial", 12)).pack()
        self.uid_var = tk.StringVar()
        self.label_uid = tk.Label(root, textvariable=self.uid_var, font=("Arial", 12, "bold"))
        self.label_uid.pack(pady=2)

        self.botao_ler = tk.Button(root, text="📖 Ler Cartão", command=self.ler_cartao)
        self.botao_ler.pack(pady=10)

        self.botao_cancelar = tk.Button(root, text="❌ Cancelar Leitura", command=self.cancelar_leitura, state=tk.DISABLED)
        self.botao_cancelar.pack(pady=5)

        self.label_dados = tk.Label(root, text="Dados lidos:", font=("Arial", 12))
        self.label_dados.pack()

        self.texto_dados = tk.Text(root, height=2, width=35, font=("Arial", 12))
        self.texto_dados.pack()

        tk.Label(root, text="Novo dado para gravar:", font=("Arial", 12)).pack(pady=10)
        self.entrada_gravar = tk.Entry(root, font=("Arial", 12), width=30)
        self.entrada_gravar.pack()

        self.botao_gravar = tk.Button(root, text="💾 Gravar no Cartão", command=self.gravar_dado)
        self.botao_gravar.pack(pady=10)

        self.botao_buscar = tk.Button(root, text="🔍 Buscar Histórico", command=self.buscar_historico)
        self.botao_buscar.pack(pady=5)

        self.botao_mostrar_tudo = tk.Button(root, text="📋 Mostrar Todo Histórico", command=self.mostrar_todo_historico)
        self.botao_mostrar_tudo.pack(pady=5)

        self.botao_exportar = tk.Button(root, text="📤 Exportar para Excel", command=self.exportar_para_excel)
        self.botao_exportar.pack(pady=5)

        self.botao_limpar = tk.Button(root, text="🧹 Limpar Histórico", command=self.limpar_historico)
        self.botao_limpar.pack(pady=5)

    # Conexão com a Esp 32
    # Tenta abrir a porta serial para falar com a Esp 32, um time.sleep(2) que
    # espera a Esp 32 reiniciar, se não conseguir, mostra erro na interface.
    def conectar_serial(self):
        try:
            self.serial = serial.Serial(PORTA_SERIAL, BAUDRATE, timeout=1)
            time.sleep(2)
        except serial.SerialException:
            messagebox.showerror("Erro", f"Não foi possível abrir a porta {PORTA_SERIAL}")

    def ler_cartao(self):
        if not self.serial or not self.serial.is_open: # Verifica se a porta serial está disponível
            messagebox.showerror("Erro", "Porta serial não conectada")
            return

        if self.leitura_em_andamento:
            messagebox.showinfo("Aviso", "Leitura já em andamento.")
            return

        self.leitura_em_andamento = True
        self.botao_ler.config(state=tk.DISABLED)
        self.botao_cancelar.config(state=tk.NORMAL)
        self.texto_dados.delete('1.0', tk.END)
        self.uid_var.set("Lendo...")

        threading.Thread(target=self.enviar_leitura).start()

    def cancelar_leitura(self):
        self.leitura_em_andamento = False
        self.botao_ler.config(state=tk.NORMAL)
        self.botao_cancelar.config(state=tk.DISABLED)
        self.uid_var.set("Leitura cancelada")

    def enviar_leitura(self):
        try:
            self.serial.reset_input_buffer()
            self.serial.write(b'0\n') # Aqui envia "0" para a Esp 32 para começar a leitura do cartão, como está no menu.

            uid = None
            dados = None
            inicio = time.time()

            while self.leitura_em_andamento and time.time() - inicio < 10:
                linha = self.serial.readline().decode('utf-8', errors='ignore').strip()
                if linha.startswith("UID:"): # Quando consegue encontrar o UID e DADOS_LIDOS separa os valores
                    uid = linha[4:]
                elif linha.startswith("DADOS_LIDOS:"):
                    dados = linha[len("DADOS_LIDOS:"):].strip()
                    break

            if not self.leitura_em_andamento:
                return

            self.uid_var.set(uid if uid else "Não identificado")
            self.texto_dados.delete('1.0', tk.END)
            self.texto_dados.insert(tk.END, dados if dados else "Não foi possível obter os dados")

            if uid and dados:
                self.salvar_historico(uid, dados) # Salva para o histórico

        except Exception as e:
            messagebox.showerror("Erro", str(e))
        finally:
            self.leitura_em_andamento = False
            self.botao_ler.config(state=tk.NORMAL)
            self.botao_cancelar.config(state=tk.DISABLED)

    def salvar_historico(self, uid, dados):
        with open("historico.csv", "a", newline='', encoding='utf-8') as file:
            writer = csv.writer(file)
            writer.writerow([datetime.now().strftime("%Y-%m-%d %H:%M:%S"), uid, dados.strip()])

    def gravar_dado(self):
        texto = self.entrada_gravar.get()
        if not texto:
            messagebox.showwarning("Aviso", "Digite algo para gravar")
            return

        if len(texto) > 16:
            messagebox.showwarning("Limite", "Máximo de 16 caracteres")
            return

        self.botao_gravar.config(state=tk.DISABLED)
        threading.Thread(target=self.enviar_gravacao, args=(texto,)).start() # Cria uma thread para escutar a
        # resposta da ESP32 sem travar a interface.

    def enviar_gravacao(self, texto):
        try:
            self.serial.reset_input_buffer()
            self.serial.write(b'1\n') # Envia para a Esp 32 a opção "1" que é para gravar os dados
            time.sleep(1)
            self.serial.write((texto.ljust(16) + "#").encode('utf-8'))

            while True:
                linha = self.serial.readline().decode('utf-8', errors='ignore').strip() # Lê linha por linha da Esp 32 via serial
                if "success" in linha.lower() or "failed" in linha.lower():
                    break

            if "success" in linha.lower():
                messagebox.showinfo("Sucesso", "Dados gravados com sucesso!")
            else:
                messagebox.showerror("Erro", "Falha na gravação.")
        except Exception as e:
            messagebox.showerror("Erro", str(e))
        finally:
            self.botao_gravar.config(state=tk.NORMAL)

    def buscar_historico(self):
        self.abrir_janela_historico(filtrar=True)

    def mostrar_todo_historico(self):
        self.abrir_janela_historico(filtrar=False)

    def abrir_janela_historico(self, filtrar=False):
        hist_win = tk.Toplevel(self.root)
        hist_win.title("Histórico de Leituras")
        hist_win.geometry("500x400")

        filtro_var = tk.StringVar()

        if filtrar:
            tk.Label(hist_win, text="Filtrar por UID ou Data:", font=("Arial", 12)).pack(pady=5)
            entrada_filtro = tk.Entry(hist_win, textvariable=filtro_var, font=("Arial", 12))
            entrada_filtro.pack(pady=5)

        tree = ttk.Treeview(hist_win, columns=("data", "uid", "dados"), show="headings")
        for col in ("data", "uid", "dados"):
            tree.heading(col, text=col.capitalize())
            tree.column(col, width=150 if col != "dados" else 180)
        tree.pack(fill=tk.BOTH, expand=True)

        def carregar():
            tree.delete(*tree.get_children())
            try:
                with open("historico.csv", newline='', encoding='utf-8') as file:
                    reader = csv.reader(file)
                    for linha in reader:
                        if len(linha) == 3:
                            if not filtrar or filtro_var.get().lower() in " ".join(linha).lower():
                                tree.insert("", tk.END, values=linha)
            except FileNotFoundError:
                messagebox.showwarning("Aviso", "Histórico ainda não foi criado.")

        if filtrar:
            tk.Button(hist_win, text="🔎 Buscar", command=carregar).pack(pady=5)
        else:
            carregar()

    def exportar_para_excel(self):
        try:
            with open("historico.csv", newline='', encoding='utf-8') as file:
                reader = csv.reader(file)
                wb = Workbook()
                ws = wb.active
                ws.title = "Histórico RFID"
                ws.append(["Data/Hora", "UID", "Dados"])
                for linha in reader:
                    if len(linha) == 3:
                        ws.append(linha)
                wb.save("historico_rfid.xlsx")
                messagebox.showinfo("Exportado", "Histórico exportado como 'historico_rfid.xlsx'.")
        except FileNotFoundError:
            messagebox.showwarning("Aviso", "Arquivo de histórico não encontrado.")

    def limpar_historico(self):
        if messagebox.askyesno("Confirmação", "Tem certeza que deseja apagar todo o histórico?"):
            try:
                open("historico.csv", "w").close()
                messagebox.showinfo("Limpo", "Histórico apagado com sucesso.")
            except Exception as e:
                messagebox.showerror("Erro", f"Erro ao limpar histórico: {e}")
# Cria a interface gráfica onde fica no loop principal do Tkinter
if __name__ == "__main__":
    root = tk.Tk()
    app = RFIDApp(root)
    root.mainloop()
