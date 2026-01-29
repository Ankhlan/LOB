/**
 * DataGrid - Professional Spreadsheet-Style Component
 * Features: Sorting, Filtering, Resizing, Selection, Export, Pagination
 */

class DataGrid {
    constructor(containerId, options = {}) {
        this.container = document.getElementById(containerId);
        this.options = {
            columns: [],
            data: [],
            pageSize: 50,
            sortable: true,
            filterable: true,
            resizable: true,
            selectable: true,
            exportable: true,
            showToolbar: true,
            showFooter: true,
            emptyMessage: 'No data available',
            onRowClick: null,
            onRowSelect: null,
            formatters: {},
            ...options
        };
        
        this.sortColumn = null;
        this.sortDirection = 'asc';
        this.filters = {};
        this.selectedRows = new Set();
        this.currentPage = 1;
        this.filteredData = [];
        this.globalSearch = '';
        
        this.init();
    }
    
    init() {
        this.render();
        this.attachEvents();
        this.applyFiltersAndSort();
    }
    
    render() {
        const html = `
            <div class="data-grid-container">
                ${this.options.showToolbar ? this.renderToolbar() : ''}
                <div class="data-grid-wrapper">
                    <table class="data-grid">
                        <thead>
                            ${this.renderHeader()}
                            ${this.options.filterable ? this.renderFilterRow() : ''}
                        </thead>
                        <tbody id="${this.container.id}-body"></tbody>
                    </table>
                </div>
                ${this.options.showFooter ? this.renderFooter() : ''}
            </div>
        `;
        this.container.innerHTML = html;
        this.tbody = document.getElementById(`${this.container.id}-body`);
    }
    
    renderToolbar() {
        const gridId = this.container.id;
        return `
            <div class="data-grid-toolbar">
                <div class="data-grid-toolbar-left">
                    <input type="text" class="data-grid-search" placeholder="Search all columns..." id="${gridId}-search">
                    <span class="data-grid-count" id="${gridId}-count">0 rows</span>
                </div>
                <div class="data-grid-toolbar-right">
                    <button class="data-grid-btn export-btn">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
                            <polyline points="7 10 12 15 17 10"/>
                            <line x1="12" y1="15" x2="12" y2="3"/>
                        </svg>
                        Export CSV
                    </button>
                    <button class="data-grid-btn clear-btn">Clear Filters</button>
                </div>
            </div>
        `;
    }
    
    renderHeader() {
        const selectAll = this.options.selectable ? 
            `<th style="width:40px"><input type="checkbox" class="row-checkbox select-all"></th>` : '';
        
        const cols = this.options.columns.map(col => `
            <th data-column="${col.field}" style="width:${col.width || 'auto'}">
                <div class="th-content">
                    <span>${col.header}</span>
                    ${this.options.sortable ? `
                        <svg class="sort-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <path d="M12 5v14M5 12l7-7 7 7"/>
                        </svg>
                    ` : ''}
                </div>
                ${this.options.resizable ? '<div class="resize-handle"></div>' : ''}
            </th>
        `).join('');
        
        return `<tr>${selectAll}${cols}</tr>`;
    }
    
    renderFilterRow() {
        const selectCell = this.options.selectable ? '<th></th>' : '';
        const filters = this.options.columns.map(col => 
            `<th><input type="text" class="filter-input" data-column="${col.field}" placeholder="Filter..."></th>`
        ).join('');
        return `<tr class="filter-row">${selectCell}${filters}</tr>`;
    }
    
    renderFooter() {
        return `
            <div class="data-grid-footer">
                <div class="data-grid-info" id="${this.container.id}-info">Showing 0 of 0</div>
                <div class="data-grid-pagination" id="${this.container.id}-pagination"></div>
            </div>
        `;
    }
    
    renderRows() {
        if (!this.filteredData.length) {
            const colspan = this.options.columns.length + (this.options.selectable ? 1 : 0);
            this.tbody.innerHTML = `<tr class="empty-row"><td colspan="${colspan}">${this.options.emptyMessage}</td></tr>`;
            this.updateInfo();
            this.updateCount();
            return;
        }
        
        const start = (this.currentPage - 1) * this.options.pageSize;
        const pageData = this.filteredData.slice(start, start + this.options.pageSize);
        
        const rows = pageData.map((row, i) => {
            const rowIndex = start + i;
            const isSelected = this.selectedRows.has(rowIndex);
            const selectCell = this.options.selectable ? 
                `<td><input type="checkbox" class="row-checkbox" data-row="${rowIndex}" ${isSelected ? 'checked' : ''}></td>` : '';
            
            const cells = this.options.columns.map(col => {
                let value = row[col.field];
                let cls = [];
                
                if (col.type === 'number') {
                    cls.push('num');
                    value = this.formatNumber(value, col.decimals);
                } else if (col.type === 'currency') {
                    cls.push('num');
                    const n = parseFloat(value);
                    if (n > 0) cls.push('positive');
                    if (n < 0) cls.push('negative');
                    value = this.formatCurrency(value, col.decimals);
                } else if (col.type === 'percent') {
                    cls.push('num');
                    const n = parseFloat(value);
                    if (n > 0) cls.push('positive');
                    if (n < 0) cls.push('negative');
                    value = this.formatPercent(value, col.decimals);
                } else if (col.type === 'badge') {
                    value = `<span class="status-badge ${String(value || '').toLowerCase()}">${value}</span>`;
                } else if (col.type === 'side') {
                    cls.push(value === 'BUY' ? 'positive' : 'negative');
                }
                
                return `<td class="${cls.join(' ')}">${value ?? ''}</td>`;
            }).join('');
            
            return `<tr data-row="${rowIndex}" class="${isSelected ? 'selected' : ''}">${selectCell}${cells}</tr>`;
        }).join('');
        
        this.tbody.innerHTML = rows;
        this.updateInfo();
        this.updatePagination();
        this.updateCount();
    }
    
    formatNumber(v, d = 2) {
        if (v == null) return '-';
        return parseFloat(v).toLocaleString('en-US', { minimumFractionDigits: d, maximumFractionDigits: d });
    }
    
    formatCurrency(v, d = 2) {
        if (v == null) return '-';
        const n = parseFloat(v);
        return (n >= 0 ? '+' : '') + n.toLocaleString('en-US', { minimumFractionDigits: d, maximumFractionDigits: d });
    }
    
    formatPercent(v, d = 2) {
        if (v == null) return '-';
        const n = parseFloat(v);
        return (n >= 0 ? '+' : '') + n.toFixed(d) + '%';
    }
    
    attachEvents() {
        // Global search
        const search = document.getElementById(`${this.container.id}-search`);
        if (search) {
            search.addEventListener('input', e => {
                this.globalSearch = e.target.value.toLowerCase();
                this.currentPage = 1;
                this.applyFiltersAndSort();
            });
        }
        
        // Column filters
        this.container.querySelectorAll('.filter-input').forEach(inp => {
            inp.addEventListener('input', e => {
                this.filters[e.target.dataset.column] = e.target.value.toLowerCase();
                this.currentPage = 1;
                this.applyFiltersAndSort();
            });
        });
        
        // Sorting
        this.container.querySelectorAll('.th-content').forEach(th => {
            th.addEventListener('click', e => {
                const col = th.closest('th').dataset.column;
                if (!col) return;
                this.sortDirection = this.sortColumn === col && this.sortDirection === 'asc' ? 'desc' : 'asc';
                this.sortColumn = col;
                this.updateSortHeaders();
                this.applyFiltersAndSort();
            });
        });
        
        // Column resizing
        this.initResizing();
        
        // Select all
        const selectAll = this.container.querySelector('.select-all');
        if (selectAll) {
            selectAll.addEventListener('change', e => this.toggleSelectAll(e.target.checked));
        }
        
        // Row selection
        this.container.addEventListener('change', e => {
            if (e.target.classList.contains('row-checkbox') && e.target.dataset.row !== undefined) {
                const idx = parseInt(e.target.dataset.row);
                e.target.checked ? this.selectedRows.add(idx) : this.selectedRows.delete(idx);
                this.updateRowSelection(idx);
                if (this.options.onRowSelect) {
                    this.options.onRowSelect(this.getSelectedData());
                }
            }
        });
        
        // Row click
        this.container.addEventListener('click', e => {
            const row = e.target.closest('tr[data-row]');
            if (row && !e.target.classList.contains('row-checkbox') && !e.target.classList.contains('cell-action')) {
                if (this.options.onRowClick) {
                    this.options.onRowClick(this.filteredData[parseInt(row.dataset.row)]);
                }
            }
        });
        
        // Export button
        const exportBtn = this.container.querySelector('.export-btn');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => this.exportCSV());
        }
        
        // Clear filters button
        const clearBtn = this.container.querySelector('.clear-btn');
        if (clearBtn) {
            clearBtn.addEventListener('click', () => this.clearFilters());
        }
    }
    
    initResizing() {
        let resizing = false, currentTh = null, startX = 0, startWidth = 0;
        
        this.container.querySelectorAll('.resize-handle').forEach(handle => {
            handle.addEventListener('mousedown', e => {
                resizing = true;
                currentTh = handle.parentElement;
                startX = e.pageX;
                startWidth = currentTh.offsetWidth;
                handle.classList.add('active');
                e.preventDefault();
            });
        });
        
        document.addEventListener('mousemove', e => {
            if (!resizing) return;
            const width = startWidth + (e.pageX - startX);
            if (width > 50) {
                currentTh.style.width = width + 'px';
            }
        });
        
        document.addEventListener('mouseup', () => {
            if (resizing) {
                resizing = false;
                document.querySelectorAll('.resize-handle').forEach(h => h.classList.remove('active'));
            }
        });
    }
    
    updateSortHeaders() {
        this.container.querySelectorAll('th[data-column]').forEach(th => {
            th.classList.remove('sort-asc', 'sort-desc');
            if (th.dataset.column === this.sortColumn) {
                th.classList.add('sort-' + this.sortDirection);
            }
        });
    }
    
    applyFiltersAndSort() {
        // Filter
        this.filteredData = this.options.data.filter(row => {
            // Global search
            if (this.globalSearch) {
                const match = Object.values(row).some(v => 
                    String(v).toLowerCase().includes(this.globalSearch)
                );
                if (!match) return false;
            }
            
            // Column filters
            for (const [col, filter] of Object.entries(this.filters)) {
                if (filter && !String(row[col] || '').toLowerCase().includes(filter)) {
                    return false;
                }
            }
            return true;
        });
        
        // Sort
        if (this.sortColumn) {
            this.filteredData.sort((a, b) => {
                let aVal = a[this.sortColumn];
                let bVal = b[this.sortColumn];
                
                // Numeric comparison
                const aNum = parseFloat(aVal);
                const bNum = parseFloat(bVal);
                if (!isNaN(aNum) && !isNaN(bNum)) {
                    return this.sortDirection === 'asc' ? aNum - bNum : bNum - aNum;
                }
                
                // String comparison
                aVal = String(aVal || '');
                bVal = String(bVal || '');
                const cmp = aVal.localeCompare(bVal);
                return this.sortDirection === 'asc' ? cmp : -cmp;
            });
        }
        
        this.renderRows();
    }
    
    updateInfo() {
        const info = document.getElementById(`${this.container.id}-info`);
        if (!info) return;
        
        const total = this.filteredData.length;
        if (total === 0) {
            info.textContent = 'No rows to display';
        } else {
            const start = (this.currentPage - 1) * this.options.pageSize + 1;
            const end = Math.min(this.currentPage * this.options.pageSize, total);
            info.textContent = `Showing ${start} to ${end} of ${total} rows`;
        }
    }
    
    updateCount() {
        const count = document.getElementById(`${this.container.id}-count`);
        if (count) {
            count.textContent = `${this.filteredData.length} rows`;
        }
    }
    
    updatePagination() {
        const pagination = document.getElementById(`${this.container.id}-pagination`);
        if (!pagination) return;
        
        const totalPages = Math.ceil(this.filteredData.length / this.options.pageSize);
        if (totalPages <= 1) {
            pagination.innerHTML = '';
            return;
        }
        
        let html = `<button class="page-btn" ${this.currentPage === 1 ? 'disabled' : ''} data-page="${this.currentPage - 1}">◀</button>`;
        
        for (let i = 1; i <= totalPages; i++) {
            if (i === 1 || i === totalPages || Math.abs(i - this.currentPage) <= 2) {
                html += `<button class="page-btn ${i === this.currentPage ? 'active' : ''}" data-page="${i}">${i}</button>`;
            } else if (Math.abs(i - this.currentPage) === 3) {
                html += '<span>...</span>';
            }
        }
        
        html += `<button class="page-btn" ${this.currentPage === totalPages ? 'disabled' : ''} data-page="${this.currentPage + 1}">▶</button>`;
        
        pagination.innerHTML = html;
        
        // Attach click handlers
        pagination.querySelectorAll('.page-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                if (!btn.disabled) {
                    this.goToPage(parseInt(btn.dataset.page));
                }
            });
        });
    }
    
    goToPage(page) {
        const totalPages = Math.ceil(this.filteredData.length / this.options.pageSize);
        if (page >= 1 && page <= totalPages) {
            this.currentPage = page;
            this.renderRows();
        }
    }
    
    toggleSelectAll(checked) {
        if (checked) {
            this.filteredData.forEach((_, i) => this.selectedRows.add(i));
        } else {
            this.selectedRows.clear();
        }
        this.renderRows();
    }
    
    updateRowSelection(idx) {
        const row = this.tbody.querySelector(`tr[data-row="${idx}"]`);
        if (row) {
            row.classList.toggle('selected', this.selectedRows.has(idx));
        }
    }
    
    clearFilters() {
        this.filters = {};
        this.globalSearch = '';
        const search = document.getElementById(`${this.container.id}-search`);
        if (search) search.value = '';
        this.container.querySelectorAll('.filter-input').forEach(input => input.value = '');
        this.currentPage = 1;
        this.applyFiltersAndSort();
    }
    
    exportCSV() {
        const headers = this.options.columns.map(c => c.header).join(',');
        const rows = this.filteredData.map(row => 
            this.options.columns.map(c => {
                let val = row[c.field] ?? '';
                // Escape CSV special characters
                if (String(val).includes(',') || String(val).includes('"') || String(val).includes('\n')) {
                    val = '"' + String(val).replace(/"/g, '""') + '"';
                }
                return val;
            }).join(',')
        ).join('\n');
        
        const csv = headers + '\n' + rows;
        const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'export_' + new Date().toISOString().slice(0, 10) + '.csv';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }
    
    // Public API
    setData(data) {
        this.options.data = data;
        this.selectedRows.clear();
        this.currentPage = 1;
        this.applyFiltersAndSort();
    }
    
    getData() {
        return this.filteredData;
    }
    
    getSelectedData() {
        return Array.from(this.selectedRows).map(i => this.filteredData[i]);
    }
    
    addRow(row) {
        this.options.data.push(row);
        this.applyFiltersAndSort();
    }
    
    removeRow(index) {
        this.options.data.splice(index, 1);
        this.selectedRows.clear();
        this.applyFiltersAndSort();
    }
    
    refresh() {
        this.applyFiltersAndSort();
    }
}

// Make globally available
window.DataGrid = DataGrid;
